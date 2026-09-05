<#
    Verifica di compilazione degli sketch di A:\obd (ESP8266, Wemos D1 mini).

    Serve solo a controllare che il codice sia corretto: non esporta nessun
    binario e non carica niente sulla board, come gli script di A:\epd e A:\rd.
    Gli oggetti intermedi stanno in A:\tmp\arduino-build\<sketch>, fuori dalla
    codebase, e servono alla compilazione incrementale.

    Uso:
      .\build.ps1                      compila CANbus_conn
      .\build.ps1 -Sketch WIFI_conn    compila l'altro sketch principale
      .\build.ps1 -Sketch can_sniffer  compila uno dei tool
      .\build.ps1 -Clean               ricompila tutto da zero
      .\build.ps1 -Dettagli            output completo del compilatore
      .\build.ps1 -Avvisi              con tutti i warning del compilatore attivi
#>
param(
    [ValidateSet('CANbus_conn', 'WIFI_conn', 'can_sniffer', 'canbox_sniffer')]
    [string] $Sketch = 'CANbus_conn',
    [switch] $Clean,
    [switch] $Dettagli,
    [switch] $Avvisi,
    [string] $Board = 'esp8266:esp8266:d1_mini_clone'
)

$Cli      = 'C:\tmp\arduino\bin\arduino-cli.exe'
$Config   = 'C:\tmp\arduino\arduino-cli.yaml'
$Progetto = 'A:\obd'
$Librerie = 'C:\tmp\arduino\user\libraries'

# Le opzioni della board sono quelle di .vscode\arduino.json, che e' la
# configurazione con cui il progetto viene compilato sull'altra macchina
$Opzioni = 'xtal=160,vt=flash,exception=disabled,stacksmash=disabled,ssl=all,' +
           'mmu=3232,non32xfer=fast,FlashMode=dout,FlashFreq=40,eesz=4M,ip=lm2f,' +
           'dbg=Disabled,lvl=None____,wipe=all,baud=115200'

# Gli header condivisi stanno nella radice del progetto, non accanto agli sketch:
# senza questo -I nessuno dei due sketch principali compila
$FlagExtra = "build.extra_flags=-I$Progetto"

# Ogni sketch sta in una cartella che si chiama gia' come il suo .ino, quindi
# qui non serve nessuna junction
$Cartelle = @{
    'CANbus_conn'    = "$Progetto\CANbus_conn"
    'WIFI_conn'      = "$Progetto\WIFI_conn"
    'can_sniffer'    = "$Progetto\tools\can_sniffer"
    'canbox_sniffer' = "$Progetto\tools\canbox_sniffer"
}
$CartellaSketch = $Cartelle[$Sketch]
# Senza --build-path arduino-cli metterebbe gli intermedi nella build cache su C:
$BuildDir = "A:\tmp\arduino-build\$Sketch"

# Librerie di terze parti che ogni sketch include, anche attraverso gli header
# condivisi: light_sensor.h porta U8g2lib.h nei due sketch principali
$LibrerieRichieste = @{
    'CANbus_conn'    = @('mcp_can', 'U8g2')
    'WIFI_conn'      = @('U8g2')
    'can_sniffer'    = @('mcp_can')
    'canbox_sniffer' = @()
}

if (-not (Test-Path $Cli)) {
    Write-Host "arduino-cli non trovato in $Cli." -ForegroundColor Red
    Write-Host 'La toolchain portable e'' descritta nella memoria di progetto build-toolchain.'
    exit 1
}

if (-not (Test-Path $CartellaSketch)) {
    Write-Host "Sketch non trovato: $CartellaSketch" -ForegroundColor Red
    exit 1
}

# Senza il core ESP8266 la compilazione fallirebbe con un errore poco parlante.
# Il config punta a un index filtrato con la sola 3.1.2 e i tool di compilazione
$coreInstallato = (& $Cli --config-file $Config core list 2>$null) -match 'esp8266:esp8266'
if (-not $coreInstallato) {
    Write-Host 'Core ESP8266 non installato.' -ForegroundColor Yellow
    Write-Host 'Per installarlo (in C:\tmp\arduino\data):'
    Write-Host "  & '$Cli' --config-file '$Config' core update-index"
    Write-Host "  & '$Cli' --config-file '$Config' core install esp8266:esp8266@3.1.2"
    Write-Host 'La 3.1.2 e'' la versione con cui il progetto viene compilato sull''altra macchina.'
    exit 1
}

# Una libreria mancante fermerebbe la compilazione su un include: meglio dirlo prima
$mancanti = @($LibrerieRichieste[$Sketch] | Where-Object {
    -not (Get-ChildItem $Librerie -Directory -Filter "$_*" -ErrorAction SilentlyContinue)
})
if ($mancanti.Count -gt 0) {
    Write-Host "Librerie mancanti per ${Sketch}: $($mancanti -join ', ')" -ForegroundColor Yellow
    Write-Host 'Per installarle:'
    foreach ($libreria in $mancanti) {
        Write-Host "  & '$Cli' --config-file '$Config' lib install '$libreria'"
    }
    Write-Host 'Se il nome esatto non venisse riconosciuto, cercalo prima con:'
    Write-Host "  & '$Cli' --config-file '$Config' lib search '$($mancanti[0])'"
    if ($mancanti -contains 'U8g2') {
        Write-Host 'Sull''altra macchina U8g2 e'' la 2.35.30.'
    }
    exit 1
}

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$argomenti = @(
    '--config-file', $Config,
    'compile',
    '-b', "${Board}:${Opzioni}",
    '--build-property', $FlagExtra,
    '--build-path', $BuildDir,
    $CartellaSketch
)
if ($Clean)    { $argomenti += '--clean' }
if ($Dettagli) { $argomenti += '--verbose' }
if ($Avvisi)   { $argomenti += @('--warnings', 'all') }

Write-Host "Compilo $Sketch per $Board" -ForegroundColor Cyan
& $Cli @argomenti
if ($LASTEXITCODE -eq 0) {
    Write-Host ''
    Write-Host "Compilazione riuscita: $Sketch e' corretto. Nessun binario esportato." -ForegroundColor Green
}
exit $LASTEXITCODE
