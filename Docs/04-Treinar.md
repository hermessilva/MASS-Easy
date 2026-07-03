# 4. Treinar (PPO na GPU)

O treino roda o loop PPO: gera transições em 16 ambientes paralelos (C++ / OpenMP), otimiza a rede de política/valor e a rede de músculos (PyTorch/CUDA), avalia e salva.

## Iniciar

```powershell
cd D:\Tootega\Source\MASS

# Do zero
powershell -ExecutionPolicy Bypass -File scripts\train.ps1

# Continuar de um checkpoint (ex.: nn\max.pt + nn\max_muscle.pt)
powershell -ExecutionPolicy Bypass -File scripts\train.ps1 -Model max

# Usar outro metadata (outro movimento/config)
powershell -ExecutionPolicy Bypass -File scripts\train.ps1 -Meta D:\...\meu_metadata.txt
```

## O que é salvo (pasta `nn\`)

| Arquivo | Quando |
|---------|--------|
| `current.pt` / `current_muscle.pt` | toda avaliação (último estado) |
| `max.pt` / `max_muscle.pt` | quando bate novo recorde de retorno médio |
| `N.pt` / `N_muscle.pt` | a cada 100 avaliações (checkpoint numerado) |

> `*_muscle.pt` só existe se `use_muscle true` no metadata.

## Ler a saída

A cada avaliação imprime um bloco:

```
# 12 === 0h:3m:20s ===
||Loss Actor               : ...
||Loss Critic              : ...
||Loss Muscle              : ...
||Avg Return per episode   : 0.78     <- métrica principal, deve SUBIR
||Avg Step per episode     : 95.0     <- passos até cair; sobe conforme aprende a ficar de pé
||Max Avg Retun So far     : 0.81 at #11
```

- **`Avg Return per episode`** subindo = aprendendo. Um walk decente costuma chegar perto de ~0.8+.
- **`Avg Step per episode`** subindo = personagem fica mais tempo em pé antes de cair.
- Uma janela do matplotlib mostra a curva de retorno em tempo real.

## Tempo esperado

- Cada iteração ~10-15 s (buffer de 2048 transições).
- Walk convergir: algumas horas na RTX 2060 SUPER.
- Limite de iterações no código: 50000 (praticamente "infinito").

## Parar

`Ctrl+C` na janela do terminal. Os `.pt` salvos até ali continuam válidos — dá para retomar com `-Model current` ou `-Model max`.

## Rodar em background e observar depois

```powershell
$env:PYTHONPATH = "D:\Tootega\Source\MASS\python\Release"
Start-Process -FilePath "D:\Tootega\Source\MASS\Deps\venv\Scripts\python.exe" `
  -ArgumentList "main.py","-d","D:\Tootega\Source\MASS\data\metadata.txt" `
  -WorkingDirectory "D:\Tootega\Source\MASS\python"
# ... depois de um tempo, ver o resultado:
powershell -ExecutionPolicy Bypass -File scripts\view.ps1 -Nn ..\nn\max.pt -MuscleNn ..\nn\max_muscle.pt
```

## Ajustes de treino (arquivo `python\main.py`, classe `PPO.__init__`)

| Parâmetro | Padrão | O que faz |
|-----------|--------|-----------|
| `num_slaves` | 16 | ambientes paralelos (mais = mais dados/iter, mais CPU/RAM) |
| `buffer_size` | 2048 | transições por iteração antes de otimizar |
| `batch_size` | 128 | minibatch da política |
| `num_epochs` | 10 | épocas de otimização da política por iteração |
| `gamma` / `lb` | 0.99 / 0.99 | desconto / lambda do GAE |
| `default_learning_rate` | 1e-4 | learning rate (Adam) |

Próximo: [05-Troubleshooting.md](05-Troubleshooting.md).
