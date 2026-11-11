# SerjoColors

Aplicativo experimental C++ para ajustes visuais em monitores no Windows 11. Consulte [HANDOFF.md](HANDOFF.md) para limitações, histórico e testes.

## Uso

Baixe `dist/SerjoColors.exe`, coloque-o em uma pasta fixa e abra-o no Windows. Os controles alteram a imagem em tempo real em SDR; segure **Comparar** para ver a imagem anterior e solte para retornar aos ajustes. Salve um perfil e use **Aplicar** para selecioná-lo para a inicialização automática. Brilho é uma transformação da imagem, não o controle físico do painel. Os drivers podem ignorar ajustes de gama. Não use com HDR ou cor crítica sem validar no equipamento.

A versão 1.2 inclui **Verificar atualização**. O programa consulta `version.txt` e, para uma versão mais recente, baixa `dist/SerjoColors.exe`, confere SHA-256 e solicita confirmação antes da substituição. O aplicativo reinicia após a atualização; a versão anterior fica ao lado com extensão `.backup`.

## Publicar nova versão

1. Incremente os números e o texto de versão em `src/version.h` (por exemplo, 1.3).
2. No Mac com MinGW-w64, execute `./build.sh`. O script compila o executável e gera `version.txt` com versão e SHA-256.
3. Publique no ramo `main`, no **mesmo commit**, o código, `version.txt` e `dist/SerjoColors.exe`. Não aumente o número sem compilar o novo executável.
4. No Windows, clique em **Verificar atualização**. A primeira troca a partir da versão antiga sem atualizador precisa ser manual.

O manifest e o binário são obtidos via HTTPS de caminhos fixos no repositório. O checksum identifica inconsistências e corrupção; não é uma assinatura digital. O executável não é assinado. A atualização requer permissão de escrita na pasta onde ele está instalado.
