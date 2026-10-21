# Nitidez — plugin de limpeza de voz para FL Studio

Plugin VST3 que remove ruído de fundo (ventilador, chiado, barulho de sala) de gravações de voz,
com um botão principal, no estilo do Clarity Vx. Usa o RNNoise (rede neural aberta da Xiph.Org).

## Controles

| Controle | O que faz |
|---|---|
| **Nitidez** | Intensidade da limpeza (0 a 100%). Comece em 60–80%. |
| **Hush** | Reduz ainda mais o ruído nos silêncios entre as frases (0 a 30 dB). |
| **Entrada** | Ganho antes do processamento. O ideal é a voz chegar com picos em torno de −12 a −6 dB. |
| **Saída** | Ganho final. |
| **Medidores** | VOZ mostra quando o plugin detecta fala; REDUÇÃO mostra quantos dB de ruído está tirando. |

Clique duas vezes num botão para voltar ao valor padrão.

## Compilar sem instalar nada no PC (GitHub)

Os servidores gratuitos do GitHub compilam o plugin para você, no Windows.

1. Crie uma conta em github.com (gratuita) e entre nela.
2. Clique em **+ → New repository**. Dê o nome `nitidez`, marque **Private** (ou Public, tanto faz) e clique em **Create repository**.
3. Descompacte o zip no seu computador. Na página do repositório, clique em **uploading an existing file**
   e arraste **o conteúdo** da pasta `Nitidez` (as pastas `Source`, `third_party`, `tests`, `.github` e os arquivos soltos).
   Depois clique em **Commit changes**.
4. Abra a aba **Actions**. O trabalho **Build Nitidez** começa sozinho (leva uns 10 minutos). Se não começar,
   clique em **Build Nitidez → Run workflow**.
5. Quando aparecer o visto verde, clique na execução e, no fim da página, em **Artifacts**, baixe **Nitidez-VST3-Windows**.
6. Extraia o zip. Você terá a pasta `Nitidez.vst3`. Copie essa pasta **inteira** para
   `C:\Program Files\Common Files\VST3\` (o Windows vai pedir permissão de administrador).
7. No FL Studio: **Options → Manage plugins → Find installed plugins**.

Se o passo 4 mostrar um X vermelho, clique na execução, abra o passo que falhou e copie a mensagem de erro.

## Como compilar no seu próprio PC (Windows)

Você só precisa seguir os passos, não precisa programar.

1. Instale o **Visual Studio Community** (2022 ou 2026, gratuito). No instalador, marque **"Desenvolvimento para desktop com C++"**.
2. Instale o **Git** (git-scm.com) se ainda não tiver.
3. Descompacte esta pasta (por exemplo em `C:\Nitidez`).
4. Abra o menu Iniciar e procure **"x64 Native Tools Command Prompt for VS"** (com o número da sua versão).
5. Nele, digite (a primeira vez baixa o JUCE da internet, leva alguns minutos):

   ```
   cd C:\Nitidez
   cmake -B build -A x64
   cmake --build build --config Release --target Nitidez_VST3
   ```

6. Quando terminar, o plugin estará em
   `build\Nitidez_artefacts\Release\VST3\Nitidez.vst3` (é uma pasta).
7. Copie essa pasta **inteira** para `C:\Program Files\Common Files\VST3\`.

## Como compilar (Mac)

Instale o Xcode e o CMake, depois, no Terminal, dentro da pasta:

```
cmake -B build -G Xcode
cmake --build build --config Release --target Nitidez_VST3
```

Copie `Nitidez.vst3` para `~/Library/Audio/Plug-Ins/VST3/`.

## Usar no FL Studio

1. Abra **Options → Manage plugins** e clique em **Find installed plugins**.
2. Coloque a pista de voz num canal do Mixer e carregue o **Nitidez** num slot de efeito.
3. Ajuste o botão **Nitidez** ouvindo a voz com o ruído.

O plugin tem **20 ms de latência**. O FL Studio compensa isso automaticamente na reprodução e na
renderização. Para monitorar ao vivo enquanto grava, prefira gravar sem o plugin e aplicar depois.

## Teste opcional do motor de áudio

Para rodar o teste automático do processamento (não precisa do JUCE):

```
cmake -B build -A x64 -DNITIDEZ_BUILD_TESTS=ON
cmake --build build --config Release --target nitidez_engine_test
build\Release\nitidez_engine_test.exe
```

## Estrutura

- `Source/NitidezEngine.*` — processamento (RNNoise, reamostragem, Hush, mistura).
- `Source/PluginProcessor.*` — parâmetros e ligação com o FL Studio.
- `Source/PluginEditor.*` — interface.
- `third_party/rnnoise` — RNNoise (licença BSD de 3 cláusulas, em `COPYING`), ajustado para compilar no Visual Studio.

## Licenças

O RNNoise é BSD. O JUCE é baixado automaticamente e é licenciado em AGPLv3 ou sob licença comercial.
Para uso pessoal não há problema; se for distribuir ou vender o plugin, leia a licença do JUCE antes.
