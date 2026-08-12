from pathlib import Path

import pdfplumber


pdf_path = Path(r"D:\migue\Descargas\CarWash_Informe_v8_Codigo_Portada.pdf")
output_path = Path(__file__).with_name("report-text.txt")

with pdfplumber.open(pdf_path) as document:
    sections = []
    for page_number, page in enumerate(document.pages, start=1):
        sections.append(f"\n===== PAGINA PDF {page_number} =====\n")
        sections.append(page.extract_text(layout=True) or "[SIN TEXTO EXTRAIBLE]")

output_path.write_text("\n".join(sections), encoding="utf-8")
print(output_path)
