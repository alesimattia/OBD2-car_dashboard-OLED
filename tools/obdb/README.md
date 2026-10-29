# Dati OBDb

I file di questa cartella derivano dal progetto [OBDb](https://github.com/OBDb), distribuito con licenza
[CC-BY-SA-4.0](https://creativecommons.org/licenses/by-sa/4.0/). La licenza consente l'adattamento sotto GPL v3,
la licenza di questo progetto; l'attribuzione resta questa pagina.

| File | Origine |
|---|---|
| `SAEJ1979.json` | `OBDb/SAEJ1979`, `signalsets/v3/default.json` al commit `06690177d2d5e11dbf82566297128d673b5a03a7` |
| `Audi-A5.json` | `OBDb/Audi-A5`, `signalsets/v3/default.json` al commit `a8214976f2bffda1b6374f4dca1fb6ce9535a1ce` |
| `Audi-A5-support-<anno>.yaml` | `OBDb/Audi-A5`, `tests/test_cases/<anno>/command_support.yaml` allo stesso commit |
| `selection.json`, `selection.txt` | selezione dei segnali interpretabili prodotta da [`../obdb_select.ps1`](../obdb_select.ps1) |

I file vanno rigenerati con `tools\obdb_select.ps1 -Download` quando cambiano i commit fissati nello script, e con
`tools\obdb_select.ps1` quando cambiano solo le regole di selezione.
