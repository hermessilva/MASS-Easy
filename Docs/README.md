# Documentação MASS (Windows)

Guia por tarefa do MASS (Muscle-Actuated Skeletal System) portado para Windows nativo (MSVC + vcpkg + PyTorch CUDA).

## Índice

| # | Tarefa | Arquivo |
|---|--------|---------|
| 1 | Requisitos e setup do ambiente (deps, vcpkg, venv) | [01-Requisitos-e-Setup.md](01-Requisitos-e-Setup.md) |
| 2 | Compilar o projeto (build + empacotar em `Dist`) | [02-Build.md](02-Build.md) |
| 3 | Executar o viewer (ver referência / política treinada) | [03-Executar-Viewer.md](03-Executar-Viewer.md) |
| 4 | Treinar (PPO na GPU) | [04-Treinar.md](04-Treinar.md) |
| 5 | Problemas comuns e soluções | [05-Troubleshooting.md](05-Troubleshooting.md) |
| 6 | Arena — editor 3D unificado (esqueleto/músculos/movimento) | [06-Editor.md](06-Editor.md) |

## Visão geral do projeto

Simulação musculoesquelética de corpo inteiro + controle por Deep RL (paper SNU MRL, SIGGRAPH 2019). 284 músculos (modelo Hill) movem um esqueleto DART; o PPO (PyTorch) aprende ativações para imitar movimentos de referência (BVH).

Três componentes:
- **`core/`** → biblioteca C++ `mss` (física DART + músculos + ambiente RL).
- **`python/`** → módulo `pymss` (pybind11) + loop de treino PPO (`main.py`, `Model.py`).
- **`render/`** → viewer OpenGL/GLUT (`render.exe`), embute Python para inferência.

## Fluxo rápido

```powershell
# 1. compilar + empacotar
powershell -ExecutionPolicy Bypass -File scripts\build-dist.ps1

# 2. ver a referência
powershell -ExecutionPolicy Bypass -File scripts\view.ps1

# 3. treinar
powershell -ExecutionPolicy Bypass -File scripts\train.ps1

# 4. ver o resultado treinado
powershell -ExecutionPolicy Bypass -File scripts\view.ps1 -Nn ..\nn\max.pt -MuscleNn ..\nn\max_muscle.pt
```

Ver também `README-Windows.md` na raiz (resumo técnico do port).
