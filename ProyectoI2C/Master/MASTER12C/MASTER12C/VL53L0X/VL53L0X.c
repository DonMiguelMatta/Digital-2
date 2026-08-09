/*
 * VL53L0X.c
 *
 * Libreria compacta VL53L0X para ATmega328P.
 *
 * Dependencias:
 *      I2C.h
 *      I2C.c
 *
 * No requiere Arduino, Wire, STSW-IMG005 ni otra API externa.
 *
 * Nota:
 * La secuencia de inicializacion usa configuraciones del VL53L0X
 * conocidas a partir de implementaciones compactas derivadas del API ST.
 */

#include "VL53L0X.h"
#include "../I2CLIB/I2CLIB.h"

#include <stddef.h>
#include <avr/pgmspace.h>
#include <util/delay.h>


/******************************************************************************
 * Registros utilizados
 ******************************************************************************/

#define REG_SYSRANGE_START                         0x00U
#define REG_SYSTEM_SEQUENCE_CONFIG                 0x01U

#define REG_SYSTEM_INTERRUPT_CONFIG_GPIO           0x0AU
#define REG_SYSTEM_INTERRUPT_CLEAR                 0x0BU

#define REG_RESULT_INTERRUPT_STATUS                0x13U
#define REG_RESULT_RANGE_STATUS                    0x14U

#define REG_FINAL_RANGE_MIN_COUNT_RATE             0x44U
#define REG_MSRC_CONFIG_TIMEOUT_MACROP             0x46U

#define REG_PRE_RANGE_CONFIG_VCSEL_PERIOD          0x50U
#define REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI     0x51U

#define REG_MSRC_CONFIG_CONTROL                    0x60U

#define REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD        0x70U
#define REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI   0x71U

#define REG_GPIO_HV_MUX_ACTIVE_HIGH                0x84U
#define REG_I2C_SLAVE_DEVICE_ADDRESS               0x8AU

#define REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV       0x89U

#define REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0       0xB0U
#define REG_GLOBAL_CONFIG_REF_EN_START_SELECT      0xB6U

#define REG_IDENTIFICATION_MODEL_ID                0xC0U

#define REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD    0x4EU
#define REG_DYNAMIC_SPAD_REF_EN_START_OFFSET       0x4FU


/******************************************************************************
 * Estructuras internas para calcular los tiempos del ranging.
 ******************************************************************************/

typedef struct
{
    uint8_t tcc;
    uint8_t dss;
    uint8_t msrc;
    uint8_t pre_range;
    uint8_t final_range;

} VL53_SequenceEnable_t;


typedef struct
{
    uint8_t pre_range_vcsel_period_pclks;
    uint8_t final_range_vcsel_period_pclks;

    uint16_t msrc_dss_tcc_mclks;
    uint16_t pre_range_mclks;
    uint16_t final_range_mclks;

    uint32_t msrc_dss_tcc_us;
    uint32_t pre_range_us;
    uint32_t final_range_us;

} VL53_SequenceTimeout_t;


/******************************************************************************
 * Secuencia compacta de configuracion.
 *
 * Se guarda en FLASH con PROGMEM para no consumir RAM del ATmega328P.
 * Cada par contiene:
 *
 *      { registro, valor }
 ******************************************************************************/

static const uint8_t VL53_tuning[][2] PROGMEM =
{
    {0xFF, 0x01},
    {0x00, 0x00},

    {0xFF, 0x00},
    {0x09, 0x00},
    {0x10, 0x00},
    {0x11, 0x00},

    {0x24, 0x01},
    {0x25, 0xFF},
    {0x75, 0x00},

    {0xFF, 0x01},
    {0x4E, 0x2C},
    {0x48, 0x00},
    {0x30, 0x20},

    {0xFF, 0x00},
    {0x30, 0x09},
    {0x54, 0x00},
    {0x31, 0x04},
    {0x32, 0x03},
    {0x40, 0x83},
    {0x46, 0x25},
    {0x60, 0x00},
    {0x27, 0x00},
    {0x50, 0x06},
    {0x51, 0x00},
    {0x52, 0x96},
    {0x56, 0x08},
    {0x57, 0x30},
    {0x61, 0x00},
    {0x62, 0x00},
    {0x64, 0x00},
    {0x65, 0x00},
    {0x66, 0xA0},

    {0xFF, 0x01},
    {0x22, 0x32},
    {0x47, 0x14},
    {0x49, 0xFF},
    {0x4A, 0x00},

    {0xFF, 0x00},
    {0x7A, 0x0A},
    {0x7B, 0x00},
    {0x78, 0x21},

    {0xFF, 0x01},
    {0x23, 0x34},
    {0x42, 0x00},
    {0x44, 0xFF},
    {0x45, 0x26},
    {0x46, 0x05},
    {0x40, 0x40},
    {0x0E, 0x06},
    {0x20, 0x1A},
    {0x43, 0x40},

    {0xFF, 0x00},
    {0x34, 0x03},
    {0x35, 0x44},

    {0xFF, 0x01},
    {0x31, 0x04},
    {0x4B, 0x09},
    {0x4C, 0x05},
    {0x4D, 0x04},

    {0xFF, 0x00},
    {0x44, 0x00},
    {0x45, 0x20},
    {0x47, 0x08},
    {0x48, 0x28},
    {0x67, 0x00},
    {0x70, 0x04},
    {0x71, 0x01},
    {0x72, 0xFE},
    {0x76, 0x00},
    {0x77, 0x00},

    {0xFF, 0x01},
    {0x0D, 0x01},

    {0xFF, 0x00},
    {0x80, 0x01},
    {0x01, 0xF8},

    {0xFF, 0x01},
    {0x8E, 0x01},
    {0x00, 0x01},

    {0xFF, 0x00},
    {0x80, 0x00}
};


/******************************************************************************
 * Prototipos internos
 ******************************************************************************/

static VL53L0X_Status_t VL53_WriteReg(
    VL53L0X_t *sensor,
    uint8_t reg,
    uint8_t value);

static VL53L0X_Status_t VL53_WriteReg16(
    VL53L0X_t *sensor,
    uint8_t reg,
    uint16_t value);

static VL53L0X_Status_t VL53_ReadReg(
    VL53L0X_t *sensor,
    uint8_t reg,
    uint8_t *value);

static VL53L0X_Status_t VL53_ReadReg16(
    VL53L0X_t *sensor,
    uint8_t reg,
    uint16_t *value);

static VL53L0X_Status_t VL53_ReadMulti(
    VL53L0X_t *sensor,
    uint8_t reg,
    uint8_t *data,
    uint8_t count);

static VL53L0X_Status_t VL53_WriteMulti(
    VL53L0X_t *sensor,
    uint8_t reg,
    const uint8_t *data,
    uint8_t count);

static VL53L0X_Status_t VL53_GetSpadInfo(
    VL53L0X_t *sensor,
    uint8_t *count,
    uint8_t *is_aperture);

static VL53L0X_Status_t VL53_ReferenceCalibration(
    VL53L0X_t *sensor,
    uint8_t vhv_byte);

static uint32_t VL53_GetTimingBudget(VL53L0X_t *sensor);

static VL53L0X_Status_t VL53_SetTimingBudget(
    VL53L0X_t *sensor,
    uint32_t budget_us);

static void VL53_GetSequenceEnables(
    VL53L0X_t *sensor,
    VL53_SequenceEnable_t *enable);

static void VL53_GetSequenceTimeouts(
    VL53L0X_t *sensor,
    const VL53_SequenceEnable_t *enable,
    VL53_SequenceTimeout_t *timeout);

static uint16_t VL53_DecodeTimeout(uint16_t value);
static uint16_t VL53_EncodeTimeout(uint32_t timeout_mclks);
static uint32_t VL53_TimeoutMclksToUs(uint16_t timeout_mclks, uint8_t vcsel_pclks);
static uint32_t VL53_TimeoutUsToMclks(uint32_t timeout_us, uint8_t vcsel_pclks);
static uint8_t VL53_GetVcselPeriod(VL53L0X_t *sensor, uint8_t final_range);


/******************************************************************************
 * Inicializar estructura
 ******************************************************************************/
void VL53L0X_ObjectInit(VL53L0X_t *sensor)
{
    if (sensor == NULL)
    {
        return;
    }

    sensor->address = VL53L0X_DEFAULT_ADDRESS;
    sensor->stop_variable = 0U;
    sensor->initialized = 0U;
    sensor->timeout_ms = VL53L0X_DEFAULT_TIMEOUT_MS;

    sensor->xshut.ddr = NULL;
    sensor->xshut.port = NULL;
    sensor->xshut.pin = NULL;
    sensor->xshut.mask = 0U;
    sensor->xshut.enabled = 0U;

    sensor->gpio1.ddr = NULL;
    sensor->gpio1.port = NULL;
    sensor->gpio1.pin = NULL;
    sensor->gpio1.mask = 0U;
    sensor->gpio1.enabled = 0U;
}


/******************************************************************************
 * Asociar XSHUT
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_AttachXSHUT(
    VL53L0X_t *sensor,
    volatile uint8_t *ddr,
    volatile uint8_t *port,
    uint8_t bit)
{
    if ((sensor == NULL) ||
        (ddr == NULL) ||
        (port == NULL) ||
        (bit > 7U))
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    sensor->xshut.ddr = ddr;
    sensor->xshut.port = port;
    sensor->xshut.mask = (uint8_t)(1U << bit);
    sensor->xshut.enabled = 1U;

    /*
     * Alta impedancia y sin pull-up interno.
     * El breakout/pull-up externo debe llevar XSHUT a HIGH.
     */
    *port &= (uint8_t)~sensor->xshut.mask;
    *ddr  &= (uint8_t)~sensor->xshut.mask;

    return VL53L0X_OK;
}


/******************************************************************************
 * Asociar GPIO1
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_AttachGPIO1(
    VL53L0X_t *sensor,
    volatile uint8_t *ddr,
    volatile uint8_t *port,
    volatile uint8_t *pin,
    uint8_t bit)
{
    if ((sensor == NULL) ||
        (ddr == NULL) ||
        (port == NULL) ||
        (pin == NULL) ||
        (bit > 7U))
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    sensor->gpio1.ddr = ddr;
    sensor->gpio1.port = port;
    sensor->gpio1.pin = pin;
    sensor->gpio1.mask = (uint8_t)(1U << bit);
    sensor->gpio1.enabled = 1U;

    /*
     * GPIO1 es una salida open-drain del sensor.
     * ATmega como entrada, sin pull-up interno de 5 V.
     */
    *port &= (uint8_t)~sensor->gpio1.mask;
    *ddr  &= (uint8_t)~sensor->gpio1.mask;

    return VL53L0X_OK;
}


/******************************************************************************
 * Reset mediante XSHUT
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_Reset(VL53L0X_t *sensor)
{
    if (sensor == NULL)
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    if (sensor->xshut.enabled)
    {
        /*
         * XSHUT LOW:
         * salida LOW.
         */
        *(sensor->xshut.port) &= (uint8_t)~sensor->xshut.mask;
        *(sensor->xshut.ddr)  |= sensor->xshut.mask;

        _delay_ms(2);

        /*
         * Liberar XSHUT:
         * entrada de alta impedancia.
         */
        *(sensor->xshut.port) &= (uint8_t)~sensor->xshut.mask;
        *(sensor->xshut.ddr)  &= (uint8_t)~sensor->xshut.mask;

        _delay_ms(3);
    }
    else
    {
        _delay_ms(3);
    }

    sensor->address = VL53L0X_DEFAULT_ADDRESS;
    sensor->initialized = 0U;

    return VL53L0X_OK;
}


/******************************************************************************
 * Comprobar sensor
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_IsConnected(VL53L0X_t *sensor)
{
    uint8_t model_id = 0U;

    if (sensor == NULL)
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    if (VL53_ReadReg(sensor, REG_IDENTIFICATION_MODEL_ID, &model_id) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }

    if (model_id != VL53L0X_EXPECTED_MODEL_ID)
    {
        return VL53L0X_ERROR_ID;
    }

    return VL53L0X_OK;
}


/******************************************************************************
 * Inicializacion
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_Init(VL53L0X_t *sensor)
{
    uint8_t value;
    uint8_t spad_count;
    uint8_t spad_is_aperture;
    uint8_t spad_map[6];

    uint8_t first_spad;
    uint8_t enabled_spads;
    uint8_t i;

    uint32_t timing_budget;

    VL53L0X_Status_t status;

    if (sensor == NULL)
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    /*
     * XSHUT permite sincronizar el sensor con el reinicio del ATmega.
     */
    status = VL53L0X_Reset(sensor);

    if (status != VL53L0X_OK)
    {
        return status;
    }

    status = VL53L0X_IsConnected(sensor);

    if (status != VL53L0X_OK)
    {
        return status;
    }

    /*
     * Configurar I/O del sensor para modo 2.8 V.
     */
    status = VL53_ReadReg(sensor, REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV, &value);
    if (status != VL53L0X_OK) return status;

    status = VL53_WriteReg(
        sensor,
        REG_VHV_CONFIG_PAD_SCL_SDA_EXTSUP_HV,
        (uint8_t)(value | 0x01U));

    if (status != VL53L0X_OK) return status;

    /*
     * Modo I2C estandar.
     */
    if (VL53_WriteReg(sensor, 0x88, 0x00) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    /*
     * Obtener variable interna utilizada al iniciar cada medicion.
     */
    if (VL53_WriteReg(sensor, 0x80, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0xFF, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0x00, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;

    if (VL53_ReadReg(sensor, 0x91, &sensor->stop_variable) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(sensor, 0x00, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0xFF, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0x80, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;

    /*
     * Deshabilitar algunos chequeos previos de limite.
     */
    if (VL53_ReadReg(sensor, REG_MSRC_CONFIG_CONTROL, &value) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(
            sensor,
            REG_MSRC_CONFIG_CONTROL,
            (uint8_t)(value | 0x12U)) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }

    /*
     * Limite de señal = 0.25 MCPS.
     * Registro usa formato fijo Q9.7:
     * 0.25 * 128 = 32.
     */
    if (VL53_WriteReg16(
            sensor,
            REG_FINAL_RANGE_MIN_COUNT_RATE,
            32U) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }

    if (VL53_WriteReg(sensor, REG_SYSTEM_SEQUENCE_CONFIG, 0xFF) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;


    /**************************************************************************
     * Obtener informacion de SPAD
     **************************************************************************/
    status = VL53_GetSpadInfo(
        sensor,
        &spad_count,
        &spad_is_aperture);

    if (status != VL53L0X_OK)
    {
        return status;
    }

    if (VL53_ReadMulti(
            sensor,
            REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0,
            spad_map,
            6U) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }

    if (VL53_WriteReg(sensor, 0xFF, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(
            sensor,
            REG_DYNAMIC_SPAD_REF_EN_START_OFFSET,
            0x00) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }

    if (VL53_WriteReg(
            sensor,
            REG_DYNAMIC_SPAD_NUM_REQUESTED_REF_SPAD,
            0x2C) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }

    if (VL53_WriteReg(sensor, 0xFF, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(
            sensor,
            REG_GLOBAL_CONFIG_REF_EN_START_SELECT,
            0xB4) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }

    first_spad = spad_is_aperture ? 12U : 0U;
    enabled_spads = 0U;

    for (i = 0U; i < 48U; i++)
    {
        if ((i < first_spad) || (enabled_spads == spad_count))
        {
            spad_map[i / 8U] &= (uint8_t)~(1U << (i % 8U));
        }
        else if ((spad_map[i / 8U] >> (i % 8U)) & 0x01U)
        {
            enabled_spads++;
        }
    }

    if (VL53_WriteMulti(
            sensor,
            REG_GLOBAL_CONFIG_SPAD_ENABLES_REF_0,
            spad_map,
            6U) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }


    /**************************************************************************
     * Aplicar configuracion de tuning
     **************************************************************************/
    for (i = 0U;
         i < (uint8_t)(sizeof(VL53_tuning) / sizeof(VL53_tuning[0]));
         i++)
    {
        uint8_t reg = pgm_read_byte(&VL53_tuning[i][0]);
        uint8_t val = pgm_read_byte(&VL53_tuning[i][1]);

        if (VL53_WriteReg(sensor, reg, val) != VL53L0X_OK)
        {
            return VL53L0X_ERROR_I2C;
        }
    }


    /**************************************************************************
     * GPIO1 = nueva medicion lista, activo LOW
     **************************************************************************/
    if (VL53_WriteReg(
            sensor,
            REG_SYSTEM_INTERRUPT_CONFIG_GPIO,
            0x04) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }

    if (VL53_ReadReg(sensor, REG_GPIO_HV_MUX_ACTIVE_HIGH, &value) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(
            sensor,
            REG_GPIO_HV_MUX_ACTIVE_HIGH,
            (uint8_t)(value & (uint8_t)~0x10U)) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }

    if (VL53_WriteReg(sensor, REG_SYSTEM_INTERRUPT_CLEAR, 0x01) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;


    /**************************************************************************
     * Mantener el timing budget actual mientras se cambia la secuencia.
     **************************************************************************/
    timing_budget = VL53_GetTimingBudget(sensor);

    if (timing_budget == 0U)
    {
        return VL53L0X_ERROR_I2C;
    }

    if (VL53_WriteReg(sensor, REG_SYSTEM_SEQUENCE_CONFIG, 0xE8) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    status = VL53_SetTimingBudget(sensor, timing_budget);

    if (status != VL53L0X_OK)
    {
        return status;
    }


    /**************************************************************************
     * Calibraciones de referencia.
     **************************************************************************/
    if (VL53_WriteReg(sensor, REG_SYSTEM_SEQUENCE_CONFIG, 0x01) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    status = VL53_ReferenceCalibration(sensor, 0x40);

    if (status != VL53L0X_OK)
    {
        return status;
    }

    if (VL53_WriteReg(sensor, REG_SYSTEM_SEQUENCE_CONFIG, 0x02) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    status = VL53_ReferenceCalibration(sensor, 0x00);

    if (status != VL53L0X_OK)
    {
        return status;
    }

    if (VL53_WriteReg(sensor, REG_SYSTEM_SEQUENCE_CONFIG, 0xE8) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    sensor->initialized = 1U;

    return VL53L0X_OK;
}


/******************************************************************************
 * Iniciar single shot
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_StartSingle(VL53L0X_t *sensor)
{
    if (sensor == NULL)
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    if (!sensor->initialized)
    {
        return VL53L0X_ERROR_NOT_INITIALIZED;
    }

    /*
     * Restaurar variable interna guardada durante Init().
     */
    if (VL53_WriteReg(sensor, 0x80, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0xFF, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0x00, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(sensor, 0x91, sensor->stop_variable) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(sensor, 0x00, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0xFF, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0x80, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(sensor, REG_SYSRANGE_START, 0x01) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    return VL53L0X_OK;
}


/******************************************************************************
 * Dato listo
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_DataReady(
    VL53L0X_t *sensor,
    uint8_t *ready)
{
    uint8_t status;

    if ((sensor == NULL) || (ready == NULL))
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    if (!sensor->initialized)
    {
        return VL53L0X_ERROR_NOT_INITIALIZED;
    }

    /*
     * GPIO1 esta configurado como activo LOW.
     */
    if (sensor->gpio1.enabled)
    {
        *ready =
            ((*(sensor->gpio1.pin) & sensor->gpio1.mask) == 0U)
            ? 1U
            : 0U;

        return VL53L0X_OK;
    }

    if (VL53_ReadReg(sensor, REG_RESULT_INTERRUPT_STATUS, &status) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }

    *ready = ((status & 0x07U) != 0U) ? 1U : 0U;

    return VL53L0X_OK;
}


/******************************************************************************
 * Leer resultado
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_ReadResult(
    VL53L0X_t *sensor,
    uint16_t *distance_mm)
{
    VL53L0X_Status_t status;

    if ((sensor == NULL) || (distance_mm == NULL))
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    if (!sensor->initialized)
    {
        return VL53L0X_ERROR_NOT_INITIALIZED;
    }

    /*
     * Distancia final:
     * RESULT_RANGE_STATUS + 10 = 0x1E
     */
    status = VL53_ReadReg16(
        sensor,
        (uint8_t)(REG_RESULT_RANGE_STATUS + 10U),
        distance_mm);

    if (status != VL53L0X_OK)
    {
        return status;
    }

    return VL53L0X_ClearInterrupt(sensor);
}


/******************************************************************************
 * Medicion bloqueante
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_ReadDistance(
    VL53L0X_t *sensor,
    uint16_t *distance_mm)
{
    uint8_t value;
    uint8_t ready;

    uint16_t elapsed;

    VL53L0X_Status_t status;

    if ((sensor == NULL) || (distance_mm == NULL))
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    status = VL53L0X_StartSingle(sensor);

    if (status != VL53L0X_OK)
    {
        return status;
    }

    /*
     * Esperar a que el bit de START sea liberado.
     */
    elapsed = 0U;

    while (1)
    {
        status = VL53_ReadReg(sensor, REG_SYSRANGE_START, &value);

        if (status != VL53L0X_OK)
        {
            return status;
        }

        if ((value & 0x01U) == 0U)
        {
            break;
        }

        if (elapsed >= sensor->timeout_ms)
        {
            return VL53L0X_ERROR_TIMEOUT;
        }

        _delay_ms(1);
        elapsed++;
    }

    /*
     * Esperar nueva medicion.
     */
    elapsed = 0U;

    while (1)
    {
        status = VL53L0X_DataReady(sensor, &ready);

        if (status != VL53L0X_OK)
        {
            return status;
        }

        if (ready)
        {
            break;
        }

        if (elapsed >= sensor->timeout_ms)
        {
            return VL53L0X_ERROR_TIMEOUT;
        }

        _delay_ms(1);
        elapsed++;
    }

    return VL53L0X_ReadResult(sensor, distance_mm);
}


/******************************************************************************
 * Limpiar interrupcion
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_ClearInterrupt(VL53L0X_t *sensor)
{
    if (sensor == NULL)
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    return VL53_WriteReg(
        sensor,
        REG_SYSTEM_INTERRUPT_CLEAR,
        0x01);
}


/******************************************************************************
 * Cambiar direccion
 ******************************************************************************/
VL53L0X_Status_t VL53L0X_SetAddress(
    VL53L0X_t *sensor,
    uint8_t new_address)
{
    VL53L0X_Status_t status;

    if (sensor == NULL)
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    /*
     * Rango normal para direccion I2C de 7 bits,
     * evitando direcciones reservadas.
     */
    if ((new_address < 0x08U) || (new_address > 0x77U))
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    status = VL53_WriteReg(
        sensor,
        REG_I2C_SLAVE_DEVICE_ADDRESS,
        (uint8_t)(new_address & 0x7FU));

    if (status == VL53L0X_OK)
    {
        sensor->address = new_address;
    }

    return status;
}


/******************************************************************************
 * Timeout
 ******************************************************************************/
void VL53L0X_SetTimeout(
    VL53L0X_t *sensor,
    uint16_t timeout_ms)
{
    if (sensor != NULL)
    {
        sensor->timeout_ms = timeout_ms;
    }
}


/******************************************************************************
 ******************************************************************************
 *
 *                      FUNCIONES I2C INTERNAS
 *
 ******************************************************************************
 ******************************************************************************/


/******************************************************************************
 * Escribir registro de 8 bits
 ******************************************************************************/
static VL53L0X_Status_t VL53_WriteReg(
    VL53L0X_t *sensor,
    uint8_t reg,
    uint8_t value)
{
    if (sensor == NULL)
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    if (!I2C_Master_Start())
    {
        return VL53L0X_ERROR_I2C;
    }

    if (!I2C_Master_Write((uint8_t)(sensor->address << 1)))
    {
        I2C_Master_Stop();
        return VL53L0X_ERROR_I2C;
    }

    if (!I2C_Master_Write(reg))
    {
        I2C_Master_Stop();
        return VL53L0X_ERROR_I2C;
    }

    if (!I2C_Master_Write(value))
    {
        I2C_Master_Stop();
        return VL53L0X_ERROR_I2C;
    }

    I2C_Master_Stop();

    return VL53L0X_OK;
}


/******************************************************************************
 * Escribir registro de 16 bits, MSB primero
 ******************************************************************************/
static VL53L0X_Status_t VL53_WriteReg16(
    VL53L0X_t *sensor,
    uint8_t reg,
    uint16_t value)
{
    uint8_t data[2];

    data[0] = (uint8_t)(value >> 8);
    data[1] = (uint8_t)value;

    return VL53_WriteMulti(sensor, reg, data, 2U);
}


/******************************************************************************
 * Leer registro de 8 bits
 ******************************************************************************/
static VL53L0X_Status_t VL53_ReadReg(
    VL53L0X_t *sensor,
    uint8_t reg,
    uint8_t *value)
{
    return VL53_ReadMulti(sensor, reg, value, 1U);
}


/******************************************************************************
 * Leer registro de 16 bits, MSB primero
 ******************************************************************************/
static VL53L0X_Status_t VL53_ReadReg16(
    VL53L0X_t *sensor,
    uint8_t reg,
    uint16_t *value)
{
    uint8_t data[2];

    VL53L0X_Status_t status;

    if (value == NULL)
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    status = VL53_ReadMulti(sensor, reg, data, 2U);

    if (status != VL53L0X_OK)
    {
        return status;
    }

    *value =
        ((uint16_t)data[0] << 8) |
         (uint16_t)data[1];

    return VL53L0X_OK;
}


/******************************************************************************
 * Escribir varios bytes
 ******************************************************************************/
static VL53L0X_Status_t VL53_WriteMulti(
    VL53L0X_t *sensor,
    uint8_t reg,
    const uint8_t *data,
    uint8_t count)
{
    uint8_t i;

    if ((sensor == NULL) ||
        ((data == NULL) && (count != 0U)))
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    if (!I2C_Master_Start())
    {
        return VL53L0X_ERROR_I2C;
    }

    if (!I2C_Master_Write((uint8_t)(sensor->address << 1)))
    {
        I2C_Master_Stop();
        return VL53L0X_ERROR_I2C;
    }

    if (!I2C_Master_Write(reg))
    {
        I2C_Master_Stop();
        return VL53L0X_ERROR_I2C;
    }

    for (i = 0U; i < count; i++)
    {
        if (!I2C_Master_Write(data[i]))
        {
            I2C_Master_Stop();
            return VL53L0X_ERROR_I2C;
        }
    }

    I2C_Master_Stop();

    return VL53L0X_OK;
}


/******************************************************************************
 * Leer varios bytes
 ******************************************************************************/
static VL53L0X_Status_t VL53_ReadMulti(
    VL53L0X_t *sensor,
    uint8_t reg,
    uint8_t *data,
    uint8_t count)
{
    uint8_t i;
    uint8_t ack;

    if ((sensor == NULL) ||
        (data == NULL) ||
        (count == 0U))
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    if (!I2C_Master_Start())
    {
        return VL53L0X_ERROR_I2C;
    }

    /*
     * SLA + W
     */
    if (!I2C_Master_Write((uint8_t)(sensor->address << 1)))
    {
        I2C_Master_Stop();
        return VL53L0X_ERROR_I2C;
    }

    /*
     * Seleccionar registro.
     */
    if (!I2C_Master_Write(reg))
    {
        I2C_Master_Stop();
        return VL53L0X_ERROR_I2C;
    }

    /*
     * Repeated START.
     */
    if (!I2C_Master_RepeatedStart())
    {
        I2C_Master_Stop();
        return VL53L0X_ERROR_I2C;
    }

    /*
     * SLA + R.
     */
    if (!I2C_Master_Write(
            (uint8_t)((sensor->address << 1) | 0x01U)))
    {
        I2C_Master_Stop();
        return VL53L0X_ERROR_I2C;
    }

    for (i = 0U; i < count; i++)
    {
        /*
         * ACK para todos menos el ultimo byte.
         */
        ack = (i < (uint8_t)(count - 1U)) ? 1U : 0U;

        if (!I2C_Master_Read(&data[i], ack))
        {
            I2C_Master_Stop();
            return VL53L0X_ERROR_I2C;
        }
    }

    I2C_Master_Stop();

    return VL53L0X_OK;
}


/******************************************************************************
 ******************************************************************************
 *
 *                  CONFIGURACION INTERNA VL53L0X
 *
 ******************************************************************************
 ******************************************************************************/


/******************************************************************************
 * Obtener numero y tipo de SPAD de referencia
 ******************************************************************************/
static VL53L0X_Status_t VL53_GetSpadInfo(
    VL53L0X_t *sensor,
    uint8_t *count,
    uint8_t *is_aperture)
{
    uint8_t temp;
    uint16_t elapsed = 0U;

    if ((count == NULL) || (is_aperture == NULL))
    {
        return VL53L0X_ERROR_PARAMETER;
    }

    if (VL53_WriteReg(sensor, 0x80, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0xFF, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0x00, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(sensor, 0xFF, 0x06) != VL53L0X_OK) return VL53L0X_ERROR_I2C;

    if (VL53_ReadReg(sensor, 0x83, &temp) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(sensor, 0x83, (uint8_t)(temp | 0x04U)) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(sensor, 0xFF, 0x07) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0x81, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0x80, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0x94, 0x6B) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0x83, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;

    /*
     * Esperar resultado.
     */
    while (1)
    {
        if (VL53_ReadReg(sensor, 0x83, &temp) != VL53L0X_OK)
            return VL53L0X_ERROR_I2C;

        if (temp != 0x00U)
        {
            break;
        }

        if (elapsed >= sensor->timeout_ms)
        {
            return VL53L0X_ERROR_TIMEOUT;
        }

        _delay_ms(1);
        elapsed++;
    }

    if (VL53_WriteReg(sensor, 0x83, 0x01) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    if (VL53_ReadReg(sensor, 0x92, &temp) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    *count = (uint8_t)(temp & 0x7FU);
    *is_aperture = (uint8_t)((temp >> 7) & 0x01U);

    if (VL53_WriteReg(sensor, 0x81, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0xFF, 0x06) != VL53L0X_OK) return VL53L0X_ERROR_I2C;

    if (VL53_ReadReg(sensor, 0x83, &temp) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(sensor, 0x83, (uint8_t)(temp & (uint8_t)~0x04U)) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(sensor, 0xFF, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0x00, 0x01) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0xFF, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;
    if (VL53_WriteReg(sensor, 0x80, 0x00) != VL53L0X_OK) return VL53L0X_ERROR_I2C;

    return VL53L0X_OK;
}


/******************************************************************************
 * Calibracion interna
 ******************************************************************************/
static VL53L0X_Status_t VL53_ReferenceCalibration(
    VL53L0X_t *sensor,
    uint8_t vhv_byte)
{
    uint8_t value;
    uint16_t elapsed = 0U;

    if (VL53_WriteReg(
            sensor,
            REG_SYSRANGE_START,
            (uint8_t)(0x01U | vhv_byte)) != VL53L0X_OK)
    {
        return VL53L0X_ERROR_I2C;
    }

    while (1)
    {
        if (VL53_ReadReg(
                sensor,
                REG_RESULT_INTERRUPT_STATUS,
                &value) != VL53L0X_OK)
        {
            return VL53L0X_ERROR_I2C;
        }

        if ((value & 0x07U) != 0U)
        {
            break;
        }

        if (elapsed >= sensor->timeout_ms)
        {
            return VL53L0X_ERROR_TIMEOUT;
        }

        _delay_ms(1);
        elapsed++;
    }

    if (VL53_WriteReg(sensor, REG_SYSTEM_INTERRUPT_CLEAR, 0x01) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    if (VL53_WriteReg(sensor, REG_SYSRANGE_START, 0x00) != VL53L0X_OK)
        return VL53L0X_ERROR_I2C;

    return VL53L0X_OK;
}


/******************************************************************************
 * Obtener configuracion de secuencia
 ******************************************************************************/
static void VL53_GetSequenceEnables(
    VL53L0X_t *sensor,
    VL53_SequenceEnable_t *enable)
{
    uint8_t config = 0U;

    (void)VL53_ReadReg(sensor, REG_SYSTEM_SEQUENCE_CONFIG, &config);

    enable->tcc         = (uint8_t)((config >> 4) & 0x01U);
    enable->dss         = (uint8_t)((config >> 3) & 0x01U);
    enable->msrc        = (uint8_t)((config >> 2) & 0x01U);
    enable->pre_range   = (uint8_t)((config >> 6) & 0x01U);
    enable->final_range = (uint8_t)((config >> 7) & 0x01U);
}


/******************************************************************************
 * Obtener VCSEL period
 ******************************************************************************/
static uint8_t VL53_GetVcselPeriod(
    VL53L0X_t *sensor,
    uint8_t final_range)
{
    uint8_t reg_value = 0U;

    if (final_range)
    {
        (void)VL53_ReadReg(
            sensor,
            REG_FINAL_RANGE_CONFIG_VCSEL_PERIOD,
            &reg_value);
    }
    else
    {
        (void)VL53_ReadReg(
            sensor,
            REG_PRE_RANGE_CONFIG_VCSEL_PERIOD,
            &reg_value);
    }

    /*
     * PCLK = (registro + 1) * 2
     */
    return (uint8_t)((reg_value + 1U) << 1);
}


/******************************************************************************
 * Obtener timeouts de cada parte de la secuencia
 ******************************************************************************/
static void VL53_GetSequenceTimeouts(
    VL53L0X_t *sensor,
    const VL53_SequenceEnable_t *enable,
    VL53_SequenceTimeout_t *timeout)
{
    uint8_t temp8 = 0U;
    uint16_t temp16 = 0U;

    timeout->pre_range_vcsel_period_pclks =
        VL53_GetVcselPeriod(sensor, 0U);

    (void)VL53_ReadReg(
        sensor,
        REG_MSRC_CONFIG_TIMEOUT_MACROP,
        &temp8);

    timeout->msrc_dss_tcc_mclks =
        (uint16_t)temp8 + 1U;

    timeout->msrc_dss_tcc_us =
        VL53_TimeoutMclksToUs(
            timeout->msrc_dss_tcc_mclks,
            timeout->pre_range_vcsel_period_pclks);

    (void)VL53_ReadReg16(
        sensor,
        REG_PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI,
        &temp16);

    timeout->pre_range_mclks =
        VL53_DecodeTimeout(temp16);

    timeout->pre_range_us =
        VL53_TimeoutMclksToUs(
            timeout->pre_range_mclks,
            timeout->pre_range_vcsel_period_pclks);

    timeout->final_range_vcsel_period_pclks =
        VL53_GetVcselPeriod(sensor, 1U);

    (void)VL53_ReadReg16(
        sensor,
        REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
        &temp16);

    timeout->final_range_mclks =
        VL53_DecodeTimeout(temp16);

    if (enable->pre_range)
    {
        timeout->final_range_mclks -= timeout->pre_range_mclks;
    }

    timeout->final_range_us =
        VL53_TimeoutMclksToUs(
            timeout->final_range_mclks,
            timeout->final_range_vcsel_period_pclks);
}


/******************************************************************************
 * Obtener timing budget
 ******************************************************************************/
static uint32_t VL53_GetTimingBudget(VL53L0X_t *sensor)
{
    VL53_SequenceEnable_t enable;
    VL53_SequenceTimeout_t timeout;

    uint32_t budget_us;

    const uint16_t start_overhead = 1910U;
    const uint16_t end_overhead = 960U;
    const uint16_t msrc_overhead = 660U;
    const uint16_t tcc_overhead = 590U;
    const uint16_t dss_overhead = 690U;
    const uint16_t pre_range_overhead = 660U;
    const uint16_t final_range_overhead = 550U;

    budget_us =
        (uint32_t)start_overhead +
        (uint32_t)end_overhead;

    VL53_GetSequenceEnables(sensor, &enable);
    VL53_GetSequenceTimeouts(sensor, &enable, &timeout);

    if (enable.tcc)
    {
        budget_us +=
            timeout.msrc_dss_tcc_us +
            tcc_overhead;
    }

    if (enable.dss)
    {
        budget_us +=
            2UL *
            (timeout.msrc_dss_tcc_us +
             dss_overhead);
    }
    else if (enable.msrc)
    {
        budget_us +=
            timeout.msrc_dss_tcc_us +
            msrc_overhead;
    }

    if (enable.pre_range)
    {
        budget_us +=
            timeout.pre_range_us +
            pre_range_overhead;
    }

    if (enable.final_range)
    {
        budget_us +=
            timeout.final_range_us +
            final_range_overhead;
    }

    return budget_us;
}


/******************************************************************************
 * Ajustar timing budget conservando la configuracion existente
 ******************************************************************************/
static VL53L0X_Status_t VL53_SetTimingBudget(
    VL53L0X_t *sensor,
    uint32_t budget_us)
{
    VL53_SequenceEnable_t enable;
    VL53_SequenceTimeout_t timeout;

    uint32_t used_budget;
    uint32_t final_timeout_us;
    uint32_t final_timeout_mclks;

    const uint16_t start_overhead = 1910U;
    const uint16_t end_overhead = 960U;
    const uint16_t msrc_overhead = 660U;
    const uint16_t tcc_overhead = 590U;
    const uint16_t dss_overhead = 690U;
    const uint16_t pre_range_overhead = 660U;
    const uint16_t final_range_overhead = 550U;

    used_budget =
        (uint32_t)start_overhead +
        (uint32_t)end_overhead;

    VL53_GetSequenceEnables(sensor, &enable);
    VL53_GetSequenceTimeouts(sensor, &enable, &timeout);

    if (enable.tcc)
    {
        used_budget +=
            timeout.msrc_dss_tcc_us +
            tcc_overhead;
    }

    if (enable.dss)
    {
        used_budget +=
            2UL *
            (timeout.msrc_dss_tcc_us +
             dss_overhead);
    }
    else if (enable.msrc)
    {
        used_budget +=
            timeout.msrc_dss_tcc_us +
            msrc_overhead;
    }

    if (enable.pre_range)
    {
        used_budget +=
            timeout.pre_range_us +
            pre_range_overhead;
    }

    if (enable.final_range)
    {
        used_budget += final_range_overhead;

        if (used_budget > budget_us)
        {
            return VL53L0X_ERROR_PARAMETER;
        }

        final_timeout_us =
            budget_us - used_budget;

        final_timeout_mclks =
            VL53_TimeoutUsToMclks(
                final_timeout_us,
                timeout.final_range_vcsel_period_pclks);

        if (enable.pre_range)
        {
            final_timeout_mclks += timeout.pre_range_mclks;
        }

        return VL53_WriteReg16(
            sensor,
            REG_FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
            VL53_EncodeTimeout(final_timeout_mclks));
    }

    return VL53L0X_OK;
}


/******************************************************************************
 * Decodificar timeout
 ******************************************************************************/
static uint16_t VL53_DecodeTimeout(uint16_t value)
{
    return (uint16_t)(
        ((value & 0x00FFU) <<
        ((value & 0xFF00U) >> 8))
        + 1U);
}


/******************************************************************************
 * Codificar timeout
 ******************************************************************************/
static uint16_t VL53_EncodeTimeout(uint32_t timeout_mclks)
{
    uint32_t ls;
    uint16_t ms;

    if (timeout_mclks == 0U)
    {
        return 0U;
    }

    ls = timeout_mclks - 1U;
    ms = 0U;

    while ((ls & 0xFFFFFF00UL) != 0U)
    {
        ls >>= 1;
        ms++;
    }

    return (uint16_t)(
        (ms << 8) |
        (ls & 0xFFU));
}


/******************************************************************************
 * Macro period del sensor, expresado en ns
 ******************************************************************************/
static uint32_t VL53_MacroPeriodNs(uint8_t vcsel_pclks)
{
    return (
        ((uint32_t)2304U *
         (uint32_t)vcsel_pclks *
         1655UL) +
        500UL) /
        1000UL;
}


/******************************************************************************
 * MCLK -> microsegundos
 ******************************************************************************/
static uint32_t VL53_TimeoutMclksToUs(
    uint16_t timeout_mclks,
    uint8_t vcsel_pclks)
{
    uint32_t macro_period_ns;

    macro_period_ns =
        VL53_MacroPeriodNs(vcsel_pclks);

    return (
        ((uint32_t)timeout_mclks *
         macro_period_ns) +
        500UL) /
        1000UL;
}


/******************************************************************************
 * microsegundos -> MCLK
 ******************************************************************************/
static uint32_t VL53_TimeoutUsToMclks(
    uint32_t timeout_us,
    uint8_t vcsel_pclks)
{
    uint32_t macro_period_ns;

    macro_period_ns =
        VL53_MacroPeriodNs(vcsel_pclks);

    return (
        (timeout_us * 1000UL) +
        (macro_period_ns / 2UL)) /
        macro_period_ns;
}