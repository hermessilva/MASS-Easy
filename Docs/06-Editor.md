# 6. Arena — editor 3D unificado (esqueleto + músculos + movimento)

**Arena** é o editor nativo C++ (GLFW + Dear ImGui + ImGuizmo + OpenGL, binário `arena.exe`) que edita os três elementos numa só visão 3D em tempo real, com física DART ao vivo, fluxo de anatomista, e um arquivo-projeto único `.mass` que desmembra para os arquivos de treino.

## Rodar

```powershell
cd D:\Tootega\Source\MASS
powershell -ExecutionPolicy Bypass -File scripts\arena.ps1                 # abre data\human.mass
powershell -ExecutionPolicy Bypass -File scripts\arena.ps1 -Mass meu.mass
```

Se ainda não existir `data\human.mass`, gere a partir dos arquivos atuais:
```powershell
build\editor\Release\arena.exe --tomass data\metadata.txt data\human.mass
```

## Layout (janelas dockáveis)
- **Viewport** — cena 3D. Ossos = caixas aramadas, juntas = eixos, músculos = polilinhas vermelhas, waypoints = pontos.
- **Cena** — árvore do esqueleto + lista de músculos.
- **Propriedades** — edita valores do item selecionado.
- **Ferramentas** — gizmo, add/remove, espelhar, simulação.
- **Validacao anatomica** — problemas detectados.
- **Atlas anatomico (OpenSim)** — referências importadas de `.osim`.
- **Treino (PPO)** — telemetria ao vivo + iniciar/parar treino.

## Navegação (Viewport)
- Arrastar **botão esquerdo** = orbitar câmera.
- Arrastar **botão direito/meio** = pan.
- **Roda** = zoom.
- **Clique esquerdo** (sem arrastar) = selecionar (waypoint > osso > músculo por proximidade).

## Selecionar e editar
- Clique no viewport ou na árvore. Botão direito na árvore de esqueleto seleciona a **junta** (em vez do corpo).
- O **gizmo** (ImGuizmo) aparece no item: Mover / Rotacionar / Escalar (Ferramentas), em Mundo/Local.
- O **painel Propriedades** edita massa, tamanho, cor, tipo de junta, limites, BVH, e — para músculos — parâmetros Hill (f0, lm, lt, pen_angle), PCSA e metadados anatômicos.
- `f0` calcula automaticamente de **PCSA × tensão específica (60 N/cm²)** ao editar o PCSA.

## Adicionar / remover
- **+ Body** cria um corpo (filho do primeiro nó).
- **Remover** (ou tecla Delete) apaga o item selecionado (osso/músculo/waypoint).
- **Duplicar musculo** copia o músculo selecionado.
- **Espelhar L↔R** cria a versão espelhada (nomes R_/L_, waypoints com X negado, corpos remapeados).
- **Ctrl+Z / Ctrl+Y** = desfazer/refazer.

## Física DART ao vivo
No painel **Ferramentas → Simulacao**:
- **Simular** liga a física (thread separada; reusa o motor validado do treino).
- **Cinematico (BVH)** = toca o movimento de referência; **Dinamico (fisica)** = simula com gravidade + forças musculares.
- **Ativacao muscular** = slider global (modo dinâmico).
- **Reset**, **Pausar**. Durante a simulação o gizmo fica desabilitado.

## Atlas anatômico (OpenSim)
- **Anatomia → Importar atlas OpenSim (.osim)** carrega parâmetros de referência (f0, fiber length, tendon slack, pennation) de modelos como gait2392 / Rajagopal2015.
- No painel **Atlas**, filtre e clique **aplicar** para copiar `f0`/`pennation` ao músculo selecionado.

## Treino integrado
- O editor abre um servidor de telemetria (asio TCP, 127.0.0.1:8765).
- **Treino → Exportar p/ data + treinar** grava os arquivos em `data\` e dispara `scripts\train.ps1`.
- O `main.py` conecta e envia reward/loss por iteração → o painel mostra o gráfico ao vivo.
- **Parar treino** encerra o processo.

## Arquivo `.mass` e export
- **Arquivo → Salvar** grava o projeto `.mass` (JSON: esqueleto + músculos + movimentos + treino + metadados anatômicos).
- **Arquivo → Exportar para treino** desmembra em `human.xml` + `muscle284.xml` + `metadata.txt`.
- **Arquivo → Importar de metadata.txt (bootstrap)** reconstrói um `.mass` a partir dos arquivos legados.

## Round-trip (validado)
`metadata.txt → .mass → export` reproduz 23 nós, 284 músculos, 1212 waypoints, 5 end-effectors idênticos, e os XMLs exportados carregam no DART/pymss com as mesmas dimensões (56 DOF, state 136, action 50).

## CLI headless
```
arena.exe --tomass <metadata.txt> <out.mass>   # bootstrap -> .mass
arena.exe --roundtrip <metadata.txt> <outdir>  # bootstrap -> XML/metadata
arena.exe <projeto.mass>                        # abre na GUI
```
