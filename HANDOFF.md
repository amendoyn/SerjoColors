# SerjoColors — handoff de desenvolvimento

**Atualização:** 11/11/2026  
**Estado:** protótipo para testes; não aprovado como versão final.  
**Ambiente de desenvolvimento:** macOS Apple Silicon, compilação cruzada MinGW-w64.  
**Destino:** Windows 11 Pro x64, HP 240 G7 com painel BOE0697 e monitor externo LG GSM5BFB.

## Objetivo

Aplicativo C++ para selecionar monitores, ajustar imagem, criar perfis por tela e reaplicar os perfis ao iniciar a sessão do Windows. O usuário fará os testes físicos no computador Windows.

## O que foi implementado

- Interface nativa Win32, com seleção de monitor, perfis e controles deslizantes.
- Enumeração de monitores e associação de perfis por identificador obtido do Windows. A identificação não foi testada com os equipamentos de destino; modos de duplicação podem ter limitações.
- Perfil padrão baseado na primeira referência de rampa capturada e persistida para cada identidade de monitor, e perfis personalizados armazenados por usuário. Arquivos de referência usam verificação de integridade.
- Ajustes experimentais por software: brilho da imagem, contraste da imagem, nível de preto, gama e canais RGB, por meio de rampas de gama. Eles não são controles físicos do painel. Alterar o preto não aumenta o contraste físico da tela.
- Teste de 15 segundos com tentativa automática de restauração da rampa anterior; botão de restauração da primeira referência capturada.
- Opção de início no logon do usuário e reaplicação dos perfis, com tentativa de reaplicação após mudança de tela e retorno de suspensão.
- Ícone na área de notificação: duplo clique abre, menu do botão direito permite abrir ou sair. Fechar a janela a oculta; o processo continua em execução até selecionar Sair.

## Limitações e trabalho pendente

- **Saturação e nitidez não implementadas.** Não há botões funcionais para elas; o aplicativo informa a indisponibilidade.
- **Brilho físico do notebook e controles DDC/CI do LG não implementados.** Os controles de brilho e contraste disponíveis transformam a imagem por software. Eles não ajustam a iluminação do painel nem as configurações internas do LG.
- Os controles de gama dependem de uma API antiga que a Microsoft desaconselha para calibração: o driver pode ignorar, sobrescrever ou alterar as rampas, inclusive mesmo quando a chamada indicar sucesso. O resultado precisa ser confirmado visualmente. O aplicativo tenta detectar modos avançados de cor e bloquear os ajustes quando ativos ou indeterminados. Essa proteção ainda não foi testada no Windows; usar apenas em SDR. Aplicativos com gerenciamento de cor ou outras ferramentas de calibração podem interferir.
- A referência padrão fica salva por monitor desde a primeira detecção bem-sucedida; não corresponde necessariamente ao padrão de fábrica. Uma alteração posterior de perfil ICC feita fora do aplicativo não atualiza essa referência, e ainda não há função de recaptura. A reversão não é garantida após encerramento forçado, falha ou troca de driver. Não usar para trabalho crítico de cor sem validar recuperação no próprio Windows.
- A identificação, aplicação independente por monitor, inicialização, suspensão, reconexão, bandeja, perfil padrão e teste temporizado **não foram executados no Windows**. Compilação não prova funcionamento.
- A interface é Win32 simples; o visual arredondado e refinado proposto ainda não foi concluído. Janela deve ser verificada na resolução e escala reais do notebook.
- A restauração automática de um teste pode falhar; anote antes o perfil/ajustes de cor do Windows. O teste é visual, não substitui medições com colorímetro.
- O executável não é assinado digitalmente. Verificar a política de execução e a procedência antes de utilizar.

## Como testar no Windows 11

1. Copiar `dist/SerjoColors.exe` para uma pasta fixa do computador. Abrir com as duas telas conectadas, em modo estendido e SDR. Se ativar inicialização automática, não mover o executável sem desativar e reativar essa opção.
2. Verificar se notebook e LG aparecem separadamente. Anotar as configurações de cor existentes do Windows antes de alterar qualquer coisa.
3. Selecionar uma tela e alterar pouco um ajuste. Clicar em **Testar por 15 segundos**, conferir qual tela mudou e verificar se ela retorna ao estado anterior ao fim do prazo. Repetir para a outra tela.
4. Criar um perfil, salvar, testar e clicar em **Aplicar**. Confirmar visualmente o resultado. Repetir por monitor; confirmar que cada tela permaneceu com seu próprio perfil.
5. Testar **Restaurar**, reabrir o aplicativo pelo ícone da bandeja, desconectar/reconectar o LG e suspender/retomar o notebook. Registrar o comportamento e eventuais mensagens.
6. Ativar **Iniciar com o Windows**, sair e entrar na conta, e verificar se o perfil certo foi aplicado a cada tela. Desativar a opção se a aplicação não se comportar como esperado.
7. Para encerrar, usar o ícone de notificação com o botão direito e escolher **Sair**. Para desativar o início automático, desmarcar a opção dentro do aplicativo antes de removê-lo.

Se a imagem ficar inadequada e a restauração não funcionar, usar as configurações de Gerenciamento de Cores do Windows e as opções de exibição ou do driver para restaurar as configurações. Evitar aplicar ajustes extremos até confirmar que a recuperação funciona.

## Arquivos e compilação

- `src/main.cpp`: código-fonte C++.
- `build.sh`: gera `dist/SerjoColors.exe` usando `x86_64-w64-mingw32-g++`.
- `%LOCALAPPDATA%\SerjoColors\profiles.ini`: perfis locais por usuário; arquivos `.ramp` na mesma pasta guardam as referências iniciais por monitor.
- Inicialização opcional: valor `SerjoColors` em `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`.

Para compilar no Mac com MinGW-w64 instalado, executar `./build.sh`. O resultado é um executável Windows x64; as bibliotecas de C++ são ligadas estaticamente e as bibliotecas do Windows são fornecidas pelo sistema. Não há instalador.

## Próximas etapas recomendadas

1. Revisar os resultados dos testes e corrigir detecção, reversão e persistência antes de confiar na aplicação automática.
2. Implementar e testar brilho físico do painel interno e suporte DDC/CI para o LG, com leituras de capacidade e operações fora da interface.
3. Investigar caminhos reais para saturação e nitidez no hardware informado, detecção de HDR, persistência segura da referência e acabamento visual. Somente anunciar compatibilidade depois de testar.

## Revisão e evidências desta entrega

- Revisão estática independente realizada por Sol 6.1, esforço alto, antes da geração final. Foram identificados riscos de referência cumulativa, HDR, identidade, restauração e gravação. O código foi ajustado; na última revisão de código, o revisor não identificou novos problemas críticos P0/P1. A verificação do retorno de gravação do perfil ativo foi adicionada após essa revisão.
- Compilação cruzada concluída sem avisos com MinGW-w64. O arquivo foi identificado como executável gráfico Windows x86-64; dependências listadas são componentes do Windows e do runtime universal do Windows.
- SHA-256 do executável: `09b25a2b0c798bf0b3de3b4085c1e276a33995f3a7d71f756d121af806db0288`.
- Nenhuma execução, captura de tela ou teste de hardware foi realizada em Windows. A revisão é estática e não constitui aprovação funcional.

---

## Continuação — versão 1.2, 11/11/2026

**Este bloco atualiza e substitui as descrições anteriores do teste temporizado e da distribuição.**

### Retorno do teste da versão anterior

O usuário informou que o executável abriu no Windows. Os efeitos eram vistos apenas ao acionar o teste de 15 segundos; alguns controles responderam, mas o brilho aparentemente não funcionou. Esse relato não confirma o funcionamento de cada tela nem a persistência de perfis.

### Alterações implementadas

- Controles de imagem são aplicados durante a movimentação, com atualizações limitadas a aproximadamente 120 ms. O botão **Segure para comparar** mostra o estado anterior enquanto pressionado e volta ao candidato ao soltar; também aceita a barra de espaço.
- O brilho da imagem agora usa uma curva diferente, com intenção de tornar a alteração mais perceptível. Continua sendo **brilho por software**, e o efeito real e a aceitação da rampa dependem do driver. A causa do relato de brilho sem efeito não foi confirmada.
- Ao mudar a topologia de telas, operações pendentes são canceladas e a identidade é verificada novamente antes de aplicar ajustes.
- A versão aparece na interface; `src/version.h` é a origem dos números e `./build.sh` gera `version.txt` com a versão e o SHA-256 do executável.
- O botão **Verificar atualização** consulta o repositório público configurado por HTTPS, compara a versão, baixa o executável e compara seu SHA-256 com o manifesto. Se o usuário confirmar, um processo auxiliar espera o aplicativo fechar, revalida o arquivo, mantém uma cópia `.backup`, substitui o executável e abre a nova versão.
- Revisão estática independente por Sol 6.1 antes da entrega: não identificou novo P1 após correções nos fluxos principais. Foi identificado risco residual local entre fechar o arquivo conferido e sua substituição; a verificação SHA-256 não é uma assinatura digital.

### Estado e limitações

- Esta versão foi compilada no Mac e **ainda não foi testada no Windows**. Confirmação de preview, comparação, brilho, atualização e comportamento com os monitores físicos está pendente.
- **Saturação, nitidez, brilho físico do HP por WMI e brilho/contraste físico do LG por DDC/CI continuam sem implementação.**
- A primeira versão entregue não possui atualizador. É necessário fechá-la pela bandeja e copiar a versão 1.2 manualmente uma vez para a mesma pasta. Futuras atualizações dependem da publicação do executável e manifesto correspondentes no GitHub.
- O controle de gama permanece experimental; se o driver ignorar a chamada, o aplicativo não consegue assegurar o efeito. O modo de cor avançado é bloqueado de forma conservadora, mas não foi verificado no Windows do usuário. Alterações externas do perfil ICC não atualizam a referência inicial salva.
- Um eventual processo de atualização requer acesso de escrita à pasta do executável. Confira o resultado em um local gravável pelo usuário.

### Próximo teste no Windows

1. Saia da versão anterior pelo ícone da bandeja e substitua o arquivo pelo `dist/SerjoColors.exe` 1.2. Abra em SDR, com os dois monitores em modo estendido.
2. Mova devagar cada controle e verifique se o efeito aparece durante o movimento, em cada tela. Teste separadamente o brilho da imagem, com valores moderados.
3. Segure e solte **Comparar** para confirmar que mostra a imagem anterior enquanto pressionado e retorna ao ajuste ao soltar. Verifique também a barra de espaço.
4. Salve e aplique perfis por monitor; valide seleção, restauração, início automático e reconexão.
5. Clique em **Verificar atualização** e confirme que informa estar atualizado. Uma futura atualização exige publicar uma versão superior e testá-la efetivamente no Windows.
6. Se algum ajuste não responder, registre em qual monitor ocorreu, qual controle e o resultado visual; não considerar a função validada apenas porque o aplicativo abriu.

---

## Continuação — versão 1.3, 11/11/2026

Foi adicionado o controle **Temperatura da cor**, com posição neutra em zero: para a esquerda, aparência mais fria; para a direita, mais quente. O ajuste modifica os canais da imagem por software e é combinado com os controles RGB já existentes. É uma correção visual relativa à referência do monitor, não uma medição ou ajuste calibrado em kelvin.

Os novos perfis guardam a temperatura; perfis anteriores de sete valores são interpretados com temperatura neutra, preservando os demais parâmetros. A aplicação na inicialização usa o mesmo formato. A seleção do perfil e a comparação continuam usando a mesma rampa de cor.

Foram criados testes locais para leitura de perfis antigos e novos, limites e direção do ajuste. A execução e o aspecto no Windows 11 com BOE0697 e GSM5BFB ainda devem ser verificados. Persistem as limitações de gamma, HDR, brilho físico, saturação e nitidez descritas acima.

Para testar: na versão 1.2, clicar em **Verificar atualização**, aceitar a 1.3 e, após reiniciar, selecionar um monitor, mover a temperatura para os dois lados e segurar **Comparar**. Salvar e reaplicar um perfil e conferir um perfil antigo. Repetir em cada tela e em SDR. Caso a verificação automática falhe, fechar pela bandeja e substituir o executável manualmente.
