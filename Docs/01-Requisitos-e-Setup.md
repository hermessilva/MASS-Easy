# 1. Requisitos e setup do ambiente

Tudo já está provisionado nesta máquina. Este documento explica o que existe e como recriar do zero.

## Requisitos de sistema

| Item | Versão nesta máquina | Observação |
|------|----------------------|-----------|
| Visual Studio | 2026 (VS18) Community | Precisa do workload **Desktop C++** (MSVC 14.51) |
| CMake | 4.2 | Mínimo 3.16 |
| Python | 3.10.11 | Instalação em `C:\Users\Hermes\AppData\Local\Programs\Python\Python310` — precisa das pastas `include\` e `libs\python310.lib` (vêm no instalador python.org) |
| GPU | NVIDIA RTX 2060 SUPER | Driver atual → treino CUDA |
| Git | qualquer recente | para clonar vcpkg |

## Estrutura de dependências (`Deps/`, ignorado pelo git)

```
Deps/
  vcpkg/    → gerenciador C++ (DART, tinyxml, pybind11, freeglut, + eigen/assimp/bullet)
  venv/     → virtualenv Python 3.10 (torch cu124, numpy, matplotlib, ipython)
```

## Recriar as dependências do zero

### a) Bibliotecas C++ via vcpkg

```powershell
cd D:\Tootega\Source\MASS
git clone https://github.com/microsoft/vcpkg Deps\vcpkg
Deps\vcpkg\bootstrap-vcpkg.bat -disableMetrics
Deps\vcpkg\vcpkg.exe install "dartsim[collision-bullet,gui,utils]" tinyxml pybind11 freeglut --triplet x64-windows
```

> ⏱️ O build do DART a partir do fonte leva ~15-20 min (compila assimp, bullet, etc.). É normal.

### b) Ambiente Python

```powershell
py -3.10 -m venv Deps\venv
Deps\venv\Scripts\python.exe -m pip install --upgrade pip setuptools wheel
Deps\venv\Scripts\python.exe -m pip install torch torchvision --index-url https://download.pytorch.org/whl/cu124
Deps\venv\Scripts\python.exe -m pip install numpy matplotlib ipython
```

## Verificar que está tudo OK

```powershell
# Torch enxerga a GPU?
Deps\venv\Scripts\python.exe -c "import torch;print(torch.__version__, torch.cuda.is_available(), torch.cuda.get_device_name(0))"
# Esperado: 2.6.0+cu124 True NVIDIA GeForce RTX 2060 SUPER

# vcpkg instalou o DART?
Deps\vcpkg\vcpkg.exe list | Select-String dartsim
```

Próximo: [02-Build.md](02-Build.md).
