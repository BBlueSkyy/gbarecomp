# Investigação do hotspot de BIOS SWI no Android

Data: 2026-10-27. Base da engine: `3886062fad7574a1aea03ed60e5f6b63dfac79cb`
(`fix/bridge-swi-return-boundary`). Base do jogo:
`a9c4678d1d2890442787c9176b775761ba5a5cae`. Instrumentação examinada:
[engine PR #3](https://github.com/BBlueSkyy/gbarecomp/pull/3), engine
`35fe3df5da67092d07b5411755eaa3ec15cac6bb`.

**Conclusão: o intervalo iniciado no vetor SWI é o hotspot observado, mas os
dados disponíveis não identificam o serviço SWI nem as rotinas internas que
consomem esse intervalo. Não foi implementado HLE de espera nem uma otimização
de produção. Não há ganho de gameplay ou de benchmark a declarar nesta branch.**

## Evidência no aparelho e limite da atribuição

O proprietário forneceu 35,9 FPS (~60% da velocidade GBA), GUEST ~20 ms/frame,
BRIDGE 13,3 ms/frame, ~1,42 milhão de instruções interpretadas/s, PPU 2,6 ms/frame
e RAM BRIDGE 0/s. As entradas principais foram:

| Entrada | Modo | Tempo por intervalo | Chamadas | Instruções |
|---|---|---:|---:|---:|
| `0x00000008` | ARM | 488,9 ms | 22 | ~1,458 milhão |
| `0x0809A4CC` | Thumb | 1,8 ms | não fornecido | não fornecido |
| `0x0810D03E` | Thumb | 1,4 ms | não fornecido | não fornecido |

Na PR #3, `PerfBridgeScope` agrupa por `(entry_pc << 1) | thumb`. A amostra não
contém `swi_imm`, número decodificado, endereço da instrução SWI chamadora,
argumentos, ou histograma dos PCs **executados dentro** do bridge. Tempo e
instruções por entrada são inclusivos: `runtime_tick()` pode executar IRQs
sincronamente, e o bridge pode chamar outras SWIs/bridges. A soma de entradas
aninhadas pode contar trabalho duas vezes; BRIDGE global evita duplicar tempo
aninhado, mas os bins HOT não são exclusivos. PPU também se sobrepõe a BRIDGE.

Consequentemente:

- Uma entrada em `0x08` não determina se a chamada era `0x04`, `0x05`, `0x1C`
  ou outro serviço não atendido por HLE.
- As ~1,458 milhão de instruções não são um histograma de instruções residentes
  na BIOS. Podem incluir caminhos de IRQ, callbacks e código guest alcançado
  durante o intervalo.
- RAM BRIDGE 0/s exclui entradas de bridge na RAM nessa amostra; não prova que
  nenhuma instrução na RAM foi executada dentro de outro bridge.
- Nem as 22 chamadas, nem a contagem de instruções, isoladamente identificam o
  serviço. O teste abaixo demonstra o mapeamento de vários números à mesma entrada.

## Cadeia confirmada no código

1. [CMakeLists.txt](../CMakeLists.txt) vincula a BIOS nativa somente se houver
   `bios_recompiled.cpp` no `GBARECOMP_GENERATED_BIOS_DIR`; caso contrário usa
   `bios_dispatch_stub.cpp`, com tabela de BIOS vazia.
2. [runtime.cpp](../src/runtime/runtime.cpp), sem `GBARECOMP_HAVE_BIOS_RECOMP`,
   força `args.bios_hle = true` e instala o hook. Isso corrige uma imprecisão do
   relatório anterior: a ausência de `[bios].hle` no TOML **não** significa HLE
   desligado nessa configuração.
3. [runtime_arm.cpp](../src/armv4t/runtime_arm.cpp), `swi_enter()`, decodifica o
   número como `imm & 0xFF` em Thumb e `(imm >> 16) & 0xFF` em ARM. O hook roda
   **antes** da entrada em SVC. Serviço atendido retorna sem despacho da BIOS.
4. [bios_hle.cpp](../src/runtime/bios_hle.cpp), `dispatch()`, não atende
   Halt/IntrWait/VBlankIntrWait, SoundDriverInit/SoundDriverMain e outros
   serviços. `default: return 0` preserva o fallback.
5. O fallback salva CPSR/SPSR e bancos, entra em SVC ARM com I=1, cobra a
   instrução SWI, registra sua continuação e despacha `0x08`. Sem entrada nativa
   ou overlay aplicável, `runtime_dispatch_miss()` chama
   [runtime_bridge_interpret()](../src/runtime/runtime_arm_default_aborts.cpp).
   A correção da PR #2 termina essa passagem no retorno de exceção SVC, sem
   interpretar desnecessariamente o restante da função chamadora.

Os workflows do jogo não geram a BIOS nativa. Ambos os workflows da **base**
continuam fixando exatamente `3886062fad7574a1aea03ed60e5f6b63dfac79cb`. Nenhum
workflow foi alterado nesta investigação; nenhuma build remota foi solicitada.

## Experimento reproduzível sem ROM/BIOS

[bios_swi_path_probe.cpp](../tests/runtime/bios_swi_path_probe.cpp) contém
instruções ARM autorais, uma tabela de jogo vazia e usa o stub de BIOS existente.
Não contém implementações reais de IntrWait/VBlankIntrWait/áudio. O vetor
autoral apenas retorna da exceção, ou, no experimento separado, escreve HALTCNT.
O experimento chama o `runtime_swi()`, o HLE, o despacho/miss e o bridge reais.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DGBARECOMP_BUILD_ORACLE=OFF
cmake --build build --target bios_swi_path_probe -j2
./build/bios_swi_path_probe
./build/bios_swi_path_probe --check-halt
```

O alvo é `EXCLUDE_FROM_ALL`. O segundo comando de execução retorna **1**, pois
expõe uma divergência existente; não é um CTest que se apresenta como aprovado.

Resultados de roteamento:

- Para **cada** número `02`, `04`, `05`, `1A`, `1C`, `FF`, 32 chamadas ARM e 32
  Thumb: hook recebe o número correto, 32 buscas do vetor `08` e 32 instruções
  interpretadas por grupo. O vetor autoral é idêntico para todos os números.
  Bits inferiores adicionais do comentário ARM não mudam o número do serviço.
- Controle com Div (`06`): HLE real produz quociente 3/resto 1 para 10/3, sem
  busca do vetor e sem instrução interpretada, tanto em ARM quanto em Thumb.
- 16 combinações de ARM/Thumb, CPSR.I, IME e IE, com oito chamadas repetidas em
  cada: o avanço real da PPU até VBlank levanta IF; o retorno SVC entrega IRQ
  somente quando os três controles permitem; apenas a IRQ entregue reconhece
  IF. A contagem inclui as quatro instruções do wrapper IRQ autoral. O teste
  verifica continuação, preservação do frame de chamada e consumo do ledger SVC.

Isso são **512 chamadas de fallback e dois controles HLE**. O experimento
confirma roteamento e limites de atribuição; não valida a semântica dos serviços
da BIOS e não reconstitui a primeira fase. Não mede um ganho antes/depois.
Os 16 casos de VBlank/IRQ usam o vetor que retorna imediatamente, **sem HALT**;
não são testes de aceitação de VBlankIntrWait. O caso HALT abaixo tem controles
de IRQ zerados; wake com IRQ pendente ou mascarada ainda precisa de validação
numa eventual implementação de espera.

## Divergência adicional comprovada: HALT dentro do bridge

O bridge não consulta HALT entre suas instruções. O código gerado verifica
`runtime_should_yield()` no prólogo; `runtime_force_interp_step()` também faz
essa verificação. O experimento `--check-halt` executa um vetor autoral que faz
`STRB` em HALTCNT, depois `ADD R2,R2,#1` e retorno SVC. IE/IF/IME estão zerados;
nenhuma IRQ acordou o processador.

| Caminho existente | PC ao terminar a tentativa | R2 | HALT | Ciclos |
|---|---:|---:|---:|---:|
| Frontend `runtime_force_interp_step` | `00000014` | 0 | 1 | 9 |
| `runtime_swi` → miss → bridge | `03001000` | 1 | 1 | 13 |

O bridge executou a instrução posterior ao HALT e o retorno SVC ainda parado.
Os quatro ciclos extras correspondem ao ADD e retorno executados indevidamente
nesta sequência autoral. Isso é uma divergência de fronteira de execução, não
uma medição de quanto tempo IntrWait consome no jogo.

Não foi aplicada uma alteração de uma linha: a saída normal do bridge consome
um frame BL, enquanto uma suspensão deve preservar esse frame e o ledger SVC.
Uma retomada em um PC interno da BIOS também não satisfaz a classificação
atual `swi_vector`, restrita à entrada `08`. Bombear eventos dentro do bridge
exige contratos de wake, IRQ/floor, apresentação/cancelamento e passes limitados.
Esses detalhes precisam ser resolvidos antes de usar esse caminho como base
para uma espera nativa. Não há evidência de que a chamada cara no aparelho
tenha passado por HALTCNT; corrigir isso como se fosse a causa medida seria
especulativo.

## Mecanismos existentes e alternativa de BIOS nativa

O runner já tem `pump_idle()`: escolhe o próximo evento PPU/timer/amostra e usa
`runtime_tick()`, sem executar instruções enquanto HALT está ativo. O runtime
já mantém ciclos pendentes, horizonte de eventos, DMA roubada, latência de
wake, entrega de IRQ, bancos/SPSR e ledger de retorno SWI. Uma solução
qualificada deve reutilizar esses mecanismos, sem delays de host, avanço
arbitrário até VBlank, alteração de IF ou escrita artificial dos flags BIOS.

O hook HLE atual é orientado a serviços que terminam antes de entrar em SVC.
Uma espera resumível não pode simplesmente devolver sucesso por esse hook e
presumir que os contratos de exceção/HALT foram cumpridos. `0x03007FF8` (flags
mantidos pelo handler guest) também não é intercambiável com IF.

Como comparação, [mGBA hle-bios.s, revisão
1509d86028c7df2d844b590723a80b2978f49e7c](https://github.com/mgba-emu/mgba/blob/1509d86028c7df2d844b590723a80b2978f49e7c/src/gba/hle-bios.s)
usa HALTCNT e um teste de flags em memória para IntrWait; não usa um sleep do
host. Isso é referência de mecanismo, não prova de temporização idêntica à BIOS
original. Nenhum trecho desse código foi incorporado ao experimento.

Recompilar a BIOS é uma alternativa tecnicamente pertinente para eliminar
faltas nativas da BIOS sem adivinhar um serviço HLE. A engine já oferece
`gba_recompile --bios`, `bios/gba_bios.toml` com `static_resume_all = true` e
`GBARECOMP_GENERATED_BIOS_DIR`; não exige regenerar os 16 shards do jogo.
Porém a geração precisa do dump privado verificado e produz fontes privadas.
Esse insumo não está disponível nesta investigação e não foi incluído,
buscado em terceiros ou substituído por uma BIOS de procedência incerta.
Nenhuma promessa de ciclos/paridade ou de ganho ARM64 decorre só desse mecanismo.

## Validação e estado da entrega

O experimento foi compilado e executado localmente em x86_64/GCC 13.3, contra
bibliotecas da base PR #2 e também em um build CMake novo desta branch. O modo
de roteamento passou com zero falhas; `--check-halt` falhou
como descrito. A revisão independente reproduziu ambos os resultados e confirmou
a lacuna de atribuição e o risco da suspensão/retomada.

Passaram os dez testes essenciais existentes: `bridge_swi_return_tests`,
`bridge_iret_tests`, `swi_snapshot_tests`, `tail_dispatch_tests`, `bus_tests`,
`timer_tests`, `dma_tests`, `irq_pending_tests`, `ppu_smoke_tests` e
`audio_playback_tests`. Foram usados os binários da base já construídos;
isso não é uma build Android/APK, nem uma validação completa de hardware.

O build GCC limpo encontra o problema preexistente em `runtime.cpp`: operador
condicional entre lambda e `nullptr` em `g_runtime_dispatch_observer`. A
compilação local aplicou conversão explícita `+[]` apenas nesse ponto e o arquivo
foi restaurado após o build; isso não faz parte desta branch e não é uma
correção de performance. O alvo novo foi construído nesse build local. Nenhum
flag de otimização do aplicativo foi alterado.

Faltam exatamente: atribuição dinâmica do intervalo caro ao **número SWI e
callsite**, distribuição dos **PCs executados** dentro dele e estado de
IE/IF/IME/CPSR/flags BIOS quando ocorre a espera, caso exista. Inspecionar
constantes SWI estáticas não provaria qual delas domina o intervalo observado.
As leituras dos shards privados grandes pelo conector retornaram conteúdo vazio
ou erro de transporte; o Git local não tinha autenticação para esse repositório.
Isso limita a inspeção estática, mas não é o motivo da impossibilidade de
atribuição dinâmica: a própria amostra da PR #3 não guarda essa informação.

O próximo passo tecnicamente decisivo é correlacionar esses campos numa
execução que o mantenedor já consiga reproduzir, ou em evidência existente
que já os contenha. Se a infraestrutura de captura existente vier a ser usada
em QA normal, a chave deve incluir número SWI/callsite e separar tempo de IRQ
e PCs internos; mais contagens de `08` não resolvem a ambiguidade. **Não foi
solicitado ao proprietário nenhum log, CSV, ADB, screenshot ou procedimento
adicional no Moto G100.** Sem esses campos, não há serviço específico comprovado
para substituir e não há comparação anterior/otimizada válida a publicar.

Esta branch contém somente documentação e um experimento opt-in. Sem alterações
de produção, não é necessário trocar a engine do jogo nem abrir uma PR de pin
sem efeito funcional. PRs #1–#4 e `main` permanecem intocadas, sem merge;
Codemagic ARM64 e shards `-O1` permanecem como na base. Nenhuma build remota foi
disparada. O slowdown continua sem uma correção comprovada nesta entrega.
