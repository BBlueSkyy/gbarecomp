# HALT no bridge: bloqueios de suspensão e retomada

Data: 2026-10-27. Base: `fix/bridge-swi-return-boundary`, engine
`3886062fad7574a1aea03ed60e5f6b63dfac79cb`.
Referências: [engine PR #5](https://github.com/BBlueSkyy/gbarecomp/pull/5),
[engine PR #6](https://github.com/BBlueSkyy/gbarecomp/pull/6) e
[jogo PR #5](https://github.com/BBlueSkyy/DragonBallAdvancedAdventureRecomp/pull/5).

**A nova evidência identifica VBlankIntrWait (`05`) como serviço caro. O bloqueio
agora é preservar a continuação ao suspender a CPU, não descobrir o número SWI.
Esta branch não contém uma correção de produção nem um ganho de FPS alegado.**

## Evidência já disponível

Na primeira fase, o proprietário mediu 33,5 FPS/~56%, GUEST 22,1 ms/frame,
BRIDGE 16,7 ms/frame, ~1,58 milhão de instruções interpretadas/s, SWI 05
~555,1 ms no intervalo, IRQ dentro do bridge ~0,1 ms/frame, 12.512 amostras HALT
e PCs internos 036C/0344/0360. Isso torna a espera BIOS um alvo fundamentado.
As amostras são aproximadas e agregadas; não convertem todas as instruções
interpretadas ou todas as amostras HALT em desperdício comprovado.

A referência PR #5 foi reutilizada, sem refazer o levantamento de roteamento:

```text
HALT path=forced pc=00000014 r2=0 halted=1 cycles=9
HALT path=bridge pc=03001000 r2=1 halted=1 cycles=13
```

`--check-halt` continua retornando 1, corretamente denunciando a divergência.
O vetor usado é autoral e não implementa VBlankIntrWait nem contém BIOS privada.
O `bios/gba_bios.toml` já documenta 0344 como escrita HALTCNT e 0348 como sua
continuação. Não foi necessário obter ou regenerar código proprietário.

## Local exato da execução indevida

Em [runtime_arm_default_aborts.cpp](../src/runtime/runtime_arm_default_aborts.cpp),
`runtime_bridge_interpret()` executa `Interpreter::step()`, sincroniza CPU,
cobra `runtime_tick(cyc)` e verifica os critérios de retorno. Se a instrução
escreveu HALTCNT e nenhuma IRQ a acordou nesse tick, o loop ainda volta ao fetch
seguinte sem verificar `bus->io().halted()`. Os frontends nativo e
`runtime_force_interp_step()` fazem essa consulta no prólogo.

Acrescentar apenas `break` não é uma correção completa:

- O teardown normal do bridge restaura o stack de entrada **menos um frame BL**.
  Uma pausa não é o retorno dessa chamada.
- Os callers gerados chamam `runtime_call_cancel_return()` ao desenrolar sua
  pilha C++; uma pausa tem de preservar as continuações guest desses callers.
- A regra `swi_vector` da PR #2 exige `entry_pc == 8` e entrada SVC. Após HALT,
  o PC de retomada pode estar dentro da BIOS em System mode. A regra deixa de
  reconhecer o retorno SVC que encerra essa SWI.

## O caminho reutilizável e seu limite

O runner comum já possui `pump_idle()` e um orçamento de um frame em
`step_once()`: avança PPU/timers/áudio através de `runtime_tick()` e volta ao host
mesmo se a CPU continuar parada. Isso deve ser reutilizado, sem sleeps de host,
instruções fictícias nem saltos arbitrários até VBlank.

O core já possui `runtime_suspend_dispatch()`, `runtime_set_return_yield()` e
`runtime_dispatch_suspended()`. O último também protege PC e impede o cancelamento
de frames guest. Eles são usados pelo scheduler multiplayer, mas o runner comum
não instala esse protocolo ao redor de seu despacho.

Uma integração precisa limpar a suspensão **depois** do unwind C++ e **antes**
de bombear idle: caso contrário, o despacho de uma IRQ despertadora também será
recusado como suspenso. Os frames preservados precisam ser marcados como sem
frame host, usando o mecanismo de captura/restauração existente fora do despacho.
O retorno SVC retomado também precisa de uma identificação que não confunda
helpers BIOS comuns e retornos de SWIs aninhadas.

Há um limite adicional: `runtime_irq()` normalmente guarda em sua própria
pilha C++ R0–R3/R12 preservados, floor/depth de retorno e sentinela de iret.
Se um bridge dentro da IRQ suspender e deixar essa função retornar ou lançar
uma exceção para o runner, esses dados não podem ser reconstruídos somente a
partir de CPSR/SPSR e bancos. Se não retornar, uma espera sem wake pode prender
o runner dentro do bridge. A suspensão precisa ter um proprietário explícito
da continuação IRQ, não só um PC salvo.

`RuntimeIrqContinuation` já oferece essa representação para o scheduler
multiplayer. Porém ativá-la no runner comum também exige captura/restauração e
serialização correspondentes. Não basta instalar o ponteiro de contexto.

## Experimento novo: snapshot perde a continuação IRQ

[halt_continuation_probe.cpp](../tests/runtime/halt_continuation_probe.cpp) usa
o `runtime_irq()` real no modo resumível para criar a continuação. Em seguida
modela uma fronteira de despacho IRQ parada em HALT. O PC 001C e HALT são
entradas autorais do experimento, não uma alegação de que o bridge de produção
já consiga suspender nesse ponto.

O teste grava um snapshot, modifica **somente** o R0 preservado na continuação
IRQ e grava outro. Os dois blobs são idênticos. Ao carregar no proprietário
recém-inicializado, o loader aceita CPU/HALT/BL, mas não recupera a continuação:

```text
snapshot_irq_continuation_omitted=1 bytes=553444
load cpu_mode=12 halted=1 BL=1 IRQ_depth=0 floor=0 continuations=0 original_R0=12345678
```

Isso demonstra uma limitação ao **reutilizar** esse mecanismo no runner comum;
não é uma regressão introduzida por esta branch nem uma alegação de que os
snapshots do multiplayer usem o serializer errado. O multiplayer possui seu
próprio serializer de `RuntimeArmContext`, incluindo `interrupts`.

O serializer comum em [snapshot.cpp](../src/debug/snapshot.cpp), `serialize_cpu`,
grava `ArmCpuState`, returns BL e ledger SWI, mas não `interrupts`, IRQ depth,
iret depth ou return floor. Seu loader valida o tamanho exato de CPU0. Uma
extensão compatível precisa de validação/migração explícita; anexar campos ao
payload sem atualizar esse contrato não preserva os saves existentes.

## Experimento novo: wake mascarado diverge entre backends

O mesmo teste mantém `IE & IF = 1`, escreve HALTCNT e chama `runtime_tick(1)` sem
executar instruções CPU. Para cada máscara abaixo, o backend legado mantém HALT:

| IME | CPSR.I | HALT após tick | Ciclos cobrados |
|---:|---:|---:|---:|
| 0 | 0 | 1 | 1 |
| 0 | 1 | 1 | 1 |
| 1 | 1 | 1 | 1 |

Em [runtime_bus_bridge.cpp](../src/runtime/runtime_bus_bridge.cpp), a remoção de
HALT está dentro do caminho de IRQ elegível, com latência de 7 ciclos. Já
[multiplayer_session.cpp](../src/runtime/multiplayer_session.cpp) acorda com
`IE & IF` e agenda 4 ciclos de wake, independentemente das máscaras de entrega.
Portanto esses dois backends existentes não fornecem uma referência idêntica
para o caso mascarado. Nenhuma das constantes foi transferida ou alterada.
Os números pertencem a caminhos diferentes; sua diferença isolada não demonstra
um erro de três ciclos nem autoriza substituir uma constante pela outra.
O ensaio não decide a latência correta no hardware: wake de HALT e entrega de
IRQ precisam ser qualificados separadamente ao estabelecer o contrato comum.

## Reprodução e verificações

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGBARECOMP_BUILD_ORACLE=OFF
cmake --build build --target halt_continuation_probe -j2
./build/halt_continuation_probe
```

O alvo é `EXCLUDE_FROM_ALL` e não entra no CTest de aceitação. Retorno 0 significa
que as **limitações existentes foram reproduzidas**, não que HALT foi corrigido.
O fonte foi compilado em x86_64/GCC 13.3 contra bibliotecas da base e pelo alvo
CMake. Para a compilação local, foi aplicado o workaround preexistente `+[]` no
condicional lambda/nullptr de `runtime.cpp` e restaurado antes da publicação;
não integra esta branch. Nenhum flag do aplicativo mudou.

Quatro testes existentes relacionados passaram: `irq_pending_tests`,
`swi_snapshot_tests`, `bridge_iret_tests`, `bridge_swi_return_tests`. A falha HALT
da PR #5 permaneceu reproduzível. Não há uma implementação otimizada para
comparar em ciclos/registradores/estado completo, nem benchmark de ganho ou
validação Android nesta entrega.

## Bloqueio delimitado e trabalho necessário

Uma correção completa envolve estas fronteiras, além do guard de HALT no bridge:

1. `runtime_arm.cpp` / `runtime_context.h`: propriedade explícita da continuação
   ao suspender durante IRQ, preservação de frames BL, floor e iret, e regra de
   retorno da SWI retomada em PC interno, inclusive aninhada.
2. `runtime.cpp` / `runtime_bus_bridge.cpp`: habilitar e encerrar o protocolo de
   suspensão na fronteira do runner; reutilizar o bombeamento idle sem bloquear
   apresentação, cancelamento, stepping ou passes limitados de validação.
3. `snapshot.cpp` e os contratos de contexto: roundtrip completo de uma pausa
   IRQ/SVC, validação de metadados e carregamento dos estados antigos suportados.
4. Contrato de wake: separar a condição de despertar da elegibilidade da IRQ e
   qualificar sua temporização antes de unificar implementações divergentes.

A aceitação precisa comparar o caminho pausado ao frontend de referência com
instruções autorais: HALT sem pendência, wake futuro/presente, IRQ mascarada por
IE/IME/CPSR.I, SWI 05 repetida, PCs internos, nested IRQ/SWI, SVC, BL, áudio/PPU/DMA
e estado serializado antes/depois de load. A SWI 05 autoral precisaria ser
declarada um workload de espera de teste, sem fingir paridade com a BIOS real.

Essa alteração atravessa execução, scheduler e formato de estado. Não foi
qualificada uma implementação pequena que cumpra todos esses contratos nesta
investigação; publicar apenas a saída de HALT da SWI no mainline deixaria casos
exigidos sem solução. Por isso não foi publicado um runtime parcial ou HLE.

O pin do jogo continua `3886062fad7574a1aea03ed60e5f6b63dfac79cb` na base; o pin
diagnóstico da PR #5 do jogo continua `d6bcb4020278c870ffdd01598a94d0bfb6bc47b0`.
Sem PR de pin sem mudança funcional, regeneração de shards, mudança de `-O1`,
merge, alteração das PRs anteriores/main ou builds remotas. Nenhum log, CSV,
ADB ou novo procedimento foi solicitado ao proprietário. O slowdown e a
correção de suspensão permanecem pendentes; não se declara resolução no Moto G100.
