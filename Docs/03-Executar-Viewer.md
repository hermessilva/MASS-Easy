# 3. Executar o viewer

O viewer (`render.exe`) abre uma janela OpenGL com o esqueleto + músculos. Ele **embute o Python** para rodar a inferência das redes treinadas, por isso precisa do `PYTHONPATH` apontando para o venv (que tem o torch/numpy).

## Modo fácil — script

```powershell
cd D:\Tootega\Source\MASS

# Só a referência (movimento BVH walk), sem rede
powershell -ExecutionPolicy Bypass -File scripts\view.ps1

# Política treinada com músculos (2 redes)
powershell -ExecutionPolicy Bypass -File scripts\view.ps1 -Nn ..\nn\max.pt -MuscleNn ..\nn\max_muscle.pt

# Modelo torque-only (1 rede, sem músculo)
powershell -ExecutionPolicy Bypass -File scripts\view.ps1 -Nn ..\nn\max.pt
```

## Rodando o pacote `Dist` direto

```powershell
$env:PYTHONPATH = "D:\Tootega\Source\MASS\Deps\venv\Lib\site-packages"
D:\Tootega\Source\MASS\Dist\x64\Release\render.exe D:\Tootega\Source\MASS\data\metadata.txt
```

Para carregar redes, acrescente os caminhos dos `.pt`:
```powershell
...\render.exe ...\metadata.txt ...\nn\max.pt ...\nn\max_muscle.pt
```

## Regras dos argumentos (o que `render.exe` espera)

| Argumentos | Efeito |
|-----------|--------|
| `metadata.txt` | só referência (sem controle aprendido) |
| `metadata.txt rede.pt` | modelo torque-only (`use_muscle false` no metadata) |
| `metadata.txt rede.pt musculo.pt` | modelo com músculos (`use_muscle true`) |

> O `metadata.txt` define `use_muscle`. Com músculo ligado, são necessárias **2** redes; sem músculo, **1**.

## Controles do teclado (na janela)

| Tecla | Ação |
|-------|------|
| `espaço` | inicia / pausa a simulação |
| `s` | um passo de simulação |
| `r` | reset |
| `f` | liga/desliga foco da câmera no personagem |
| `o` | liga/desliga as malhas OBJ (senão mostra formas primitivas) |
| mouse | orbitar / zoom (trackball) |
| `Esc` | sair |

> **Sempre feche com `Esc` ou fechando a janela.** Enquanto aberta, a janela aparece como "Running in the background" (é o `glutMainLoop`, comportamento normal de GUI).

Próximo: [04-Treinar.md](04-Treinar.md).
