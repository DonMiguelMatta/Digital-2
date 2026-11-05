"""Flujo local equivalente a compactar HTML y convertirlo en cadena C/C++."""
from pathlib import Path
import json,re
root=Path(__file__).resolve().parents[1]
original=(root/'web/index.html').read_text(encoding='utf-8')
# Esta pagina no contiene bloques pre/textarea ni comentarios JS de una linea.
# Se conserva el espacio interno de cadenas y texto; solo se compacta la sangria.
compacto=' '.join(line.strip() for line in original.splitlines() if line.strip())
compacto=re.sub(r'>\s+<','><',compacto)
(root/'web/index.min.html').write_text(compacto,encoding='utf-8')
literal=json.dumps(compacto,ensure_ascii=False)
header='// Generado desde web/index.html. No editar; ejecutar herramientas/preparar_html.py.\n#pragma once\n#include <Arduino.h>\nstatic const char PAGINA_HTML[] PROGMEM =\n'+literal+';\n'
(root/'Parqueo_WebServer/pagina_web.h').write_text(header,encoding='utf-8')
assert json.loads(literal)==compacto
script=re.search(r'<script>(.*?)</script>',original,re.S).group(1)
(root/'web/pagina_check.js').write_text(script,encoding='utf-8')
print('HTML original:',len(original.encode()),'bytes; compacto:',len(compacto.encode()),'bytes. Cadena C++ verificada.')
