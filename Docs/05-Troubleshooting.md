# 5. Problemas comuns e soluções

## Build

### `Could NOT find Python (missing: Development.Module)`
O `FindPython` não achou headers/libs por causa do wrapper de python do vcpkg. Passe os 3 flags explícitos (o `build.ps1`/`build-dist.ps1` já fazem):
```
-DPython_EXECUTABLE=<py310>\python.exe
-DPython_INCLUDE_DIR=<py310>\include
-DPython_LIBRARY=<py310>\libs\python310.lib
```
Confirme que `...\Python310\include\Python.h` e `...\Python310\libs\python310.lib` existem (vêm no instalador python.org; se faltar, reinstale marcando "Download debug/dev symbols").

### `Compatibility with CMake < 3.5 has been removed`
Algum `CMakeLists.txt` com `cmake_minimum_required(VERSION 2.8...)`. Todos os 4 já foram atualizados para 3.16. Se voltar, é arquivo novo — bumpe a versão.

### `'iota': is not a member of 'std'` (ou `sort`, etc.)
MSVC não puxa headers transitivamente como o GCC. Adicione o include explícito (`#include <numeric>`, `<algorithm>`).

### DART/tinyxml/pybind não encontrados
Cache de build antigo apontando para paths errados. Delete `build\` e reconfigure:
```powershell
Remove-Item build -Recurse -Force
powershell -ExecutionPolicy Bypass -File scripts\build-dist.ps1
```

## Execução do viewer

### `Failed loading mesh 'file://...'` / `file://file:///...`
Bug de URI com letra de drive no Windows (`D:` vira "scheme"). Já corrigido em `core/DARTHelper.cpp` usando `Uri::createFromPath(...)` + `LocalResourceRetriever`. Se aparecer, recompile o `mss`/`render`.

### `ModuleNotFoundError: No module named 'torch'` ao abrir o viewer
O `render.exe` embute Python e precisa do `PYTHONPATH` no venv:
```powershell
$env:PYTHONPATH = "D:\Tootega\Source\MASS\Deps\venv\Lib\site-packages"
```
O `view.ps1` já define isso.

### Crash ao abrir: `Unhandled exception ... AbortHandler.h` / `THPGenerator_initDefaultGenerator`
Conflito de runtime OpenMP: o torch traz o próprio `libiomp5md.dll` e o processo do render/DART já tem um OpenMP carregado → o torch aborta no `import`. Defina antes de rodar:
```powershell
$env:KMP_DUPLICATE_LIB_OK = "TRUE"
```
O `view.ps1` e o `train.ps1` já definem isso.

### A janela não aparece / fecha na hora
- Confira o driver da GPU / OpenGL.
- Rode direto o `render.exe` sem argumentos para ver a mensagem de uso.
- Veja se faltam DLLs ao lado do `.exe` (no `Dist` devem estar as 8 DLLs).

## Execução do treino

### `ModuleNotFoundError: No module named 'pymss'`
`main.py` precisa do módulo compilado no path:
```powershell
$env:PYTHONPATH = "D:\Tootega\Source\MASS\python\Release"
```
O `train.ps1` já define isso.

### `ValueError: setting an array element with a sequence ... inhomogeneous shape`
Incompatibilidade com NumPy 2.x. Já corrigido em `main.py` (`np.array(..., dtype=object)`). Se voltar em outro ponto, aplique o mesmo `dtype=object`.

### `UserWarning: The torch.cuda.*DtypeTensor constructors are no longer recommended`
Apenas aviso, não quebra. Vem do uso de `torch.cuda.FloatTensor` (estilo antigo). Pode ignorar ou modernizar depois.

## "Running in the background (N)" não zera

Comportamento da UI, não processo real. GUI (`render.exe`) e o treino (`main.py`) rodam até você fechar/`Ctrl+C`. Para conferir o que está vivo de verdade:
```powershell
Get-Process render,python -ErrorAction SilentlyContinue | Select-Object Name,Id
```
Se não listar nada, está tudo encerrado (a UI só não atualizou o contador).
