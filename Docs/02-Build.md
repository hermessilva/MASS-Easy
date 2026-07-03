# 2. Compilar o projeto

## Opção A — build + empacotar (recomendado)

Gera os binários **e** copia o viewer + DLLs + `pymss.pyd` para `Dist\x64\<Config>`:

```powershell
cd D:\Tootega\Source\MASS
powershell -ExecutionPolicy Bypass -File scripts\build-dist.ps1              # Release (padrão)
powershell -ExecutionPolicy Bypass -File scripts\build-dist.ps1 -Config Debug
```

Resultado:

```
Dist\x64\Release\
  render.exe
  pymss.cp310-win_amd64.pyd
  assimp-vc145-mt.dll  ccd.dll  freeglut.dll  kubazip.dll
  minizip.dll  poly2tri.dll  pugixml.dll  z.dll
```

> O `Dist` é um pacote autocontido do viewer (todas as DLLs juntas). Só falta o `PYTHONPATH` apontando para o venv em tempo de execução (ver [03](03-Executar-Viewer.md)).

## Opção B — só compilar (sem empacotar)

```powershell
powershell -ExecutionPolicy Bypass -File scripts\build.ps1
```

Saídas ficam nas pastas de build do CMake:
- `core\Release\mss.lib`
- `python\Release\pymss.cp310-win_amd64.pyd`
- `build\render\Release\render.exe`

## O que o script faz por dentro

1. **Configura** (só na 1ª vez, se `build\CMakeCache.txt` não existir):
   ```powershell
   cmake -S . -B build -G "Visual Studio 18 2026" -A x64 `
     -DCMAKE_TOOLCHAIN_FILE=Deps\vcpkg\scripts\buildsystems\vcpkg.cmake `
     -DVCPKG_TARGET_TRIPLET=x64-windows `
     -DPython_EXECUTABLE=<py310>\python.exe `
     -DPython_INCLUDE_DIR=<py310>\include `
     -DPython_LIBRARY=<py310>\libs\python310.lib
   ```
   > Os 3 flags `Python_*` explícitos são obrigatórios — sem eles o `FindPython` falha em `Development.Module` por causa do wrapper de python do vcpkg.

2. **Compila**: `cmake --build build --config <Config>`.

3. **Empacota**: copia `render.exe` + `*.dll` + `pymss.pyd` para `Dist\x64\<Config>`.

## Recompilar após editar código

- Editou `core/`, `render/` ou `python/*.cpp` → rode o build de novo (a configuração é reaproveitada).
- Editou só `.py` (`main.py`, `Model.py`) → **não precisa compilar**, é interpretado.

## Targets individuais (opcional)

```powershell
cmake --build build --config Release --target mss      # só a lib core
cmake --build build --config Release --target pymss    # só o módulo Python
cmake --build build --config Release --target render   # só o viewer
```

Próximo: [03-Executar-Viewer.md](03-Executar-Viewer.md).
