from pathlib import Path
import tempfile
import unittest

from tools.install_log_analyzer import analyze_install_log, format_summary


SAMPLE_LOG = """\
[transferencia-switch] START file="Demo OK.nsp" type=NSP expected=100
[transferencia-switch] OK file="Demo OK.nsp" type=NSP received=100 cnmt=1 nca=3 existing_nca=0 detail="NSP instalado correctamente en SD"
[transferencia-switch] START file="Demo Old Installed.nsp" type=NSP expected=150
[transferencia-switch] OK file="Demo Old Installed.nsp" type=NSP received=150 cnmt=1 nca=0
[transferencia-switch] START file="Demo Installed.xci" type=XCI expected=200
[transferencia-switch] ENTRY existing file="Demo Installed.xci" name="abc.nca" size=123 detail="contenido ya instalado"
[transferencia-switch] OK file="Demo Installed.xci" type=XCI received=200 cnmt=1 nca=0 existing_nca=4 detail="XCI ya instalado; contenido existente reutilizado"
[transferencia-switch] START file="Demo Fail.xci" type=XCI expected=300
[transferencia-switch] ERROR file="Demo Fail.xci" type=XCI received=50/300 data_pos=0 entry=0/0 detail="particion secure fuera del XCI"
[transferencia-switch] CANCEL file="Demo Fail.xci" type=XCI received=60/300 detail="particion secure fuera del XCI"
[transferencia-switch] START file="Demo Cancel.xci" type=XCI expected=400
[transferencia-switch] CANCEL file="Demo Cancel.xci" type=XCI received=128/400 detail="cancelado por usuario"
"""


class InstallLogAnalyzerTests(unittest.TestCase):
    def test_analyzes_ok_already_installed_failed_and_cancelled(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "install.log"
            path.write_text(SAMPLE_LOG, encoding="utf-8")

            records = analyze_install_log(path)
            summary = format_summary(records)

            self.assertEqual(len(records), 5)
            self.assertIn("OK nuevos: 1", summary)
            self.assertIn("Ya instalados/reutilizados: 2", summary)
            self.assertIn("Fallidos: 1", summary)
            self.assertIn("Cancelados: 1", summary)
            self.assertIn("particion secure fuera del XCI", summary)
            self.assertIn("contenido existente reutilizado", summary)
            self.assertIn("log antiguo sin existing_nca", summary)


if __name__ == "__main__":
    unittest.main()
