<#
    Verifica di compilazione degli sketch di A:\obd (ESP8266, Wemos D1 mini).

    Serve solo a controllare che il codice sia corretto: non esporta nessun
    binario e non carica niente sulla board, come gli script di A:\epd e A:\rd.
    Gli oggetti intermedi restano nella cartella build\ dello sketch.

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

$Cli      = 'A:\tmp\arduino\bin\arduino-cli.exe'
$Config   = 'A:\tmp\arduino\arduino-cli.yaml'
$Progetto = 'A:\obd'
$Librerie = 'A:\tmp\arduino\user\libraries'
$IndiceEsp8266 = 'http://arduino.esp8266.com/stable/package_esp8266com_index.json'

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

if (-not (Test-Path $Cli)) {
    Write-Host "arduino-cli non trovato in $Cli." -ForegroundColor Red
    Write-Host 'La toolchain portable e'' descritta nella memoria di progetto build-toolchain.'
    exit 1
}

if (-not (Test-Path $CartellaSketch)) {
    Write-Host "Sketch non trovato: $CartellaSketch" -ForegroundColor Red
    exit 1
}

# Il core ESP8266 non fa parte della toolchain installata qui, che ha solo
# l'ESP32: senza, la compilazione fallirebbe con un errore poco parlante
$coreInstallato = (& $Cli --config-file $Config core list 2>$null) -match 'esp8266:esp8266'
if (-not $coreInstallato) {
    Write-Host 'Core ESP8266 non installato.' -ForegroundColor Yellow
    Write-Host 'Per installarlo (circa 400 MB in C:\xz\arduino\data):'
    Write-Host "  & '$Cli' --config-file '$Config' --additional-urls '$IndiceEsp8266' core update-index"
    Write-Host "  & '$Cli' --config-file '$Config' --additional-urls '$IndiceEsp8266' core install esp8266:esp8266@3.1.2"
    Write-Host 'La 3.1.2 e'' la versione con cui il progetto viene compilato sull''altra macchina.'
    exit 1
}

# Le due librerie di terze parti che gli sketch principali includono. I tool in
# tools\ non le usano, quindi per quelli il controllo si salta
if ($Sketch -eq 'CANbus_conn' -or $Sketch -eq 'WIFI_conn') {
    $mancanti = @()
    if (-not (Test-Path "$Librerie\mcp_can") -and $Sketch -eq 'CANbus_conn') {
        $mancanti += 'mcp_can'
    }
    if (-not (Get-ChildItem $Librerie -Directory -Filter 'U8g2*' -ErrorAction SilentlyContinue)) {
        $mancanti += 'U8g2'
    }
    if ($mancanti.Count -gt 0) {
        Write-Host "Librerie mancanti: $($mancanti -join ', ')" -ForegroundColor Yellow
        Write-Host 'Per installarle:'
        foreach ($libreria in $mancanti) {
            Write-Host "  & '$Cli' --config-file '$Config' lib install '$libreria'"
        }
        Write-Host 'Se il nome esatto non venisse riconosciuto, cercalo prima con:'
        Write-Host "  & '$Cli' --config-file '$Config' lib search mcp_can"
        Write-Host 'Sull''altra macchina U8g2 e'' la 2.35.30.'
        exit 1
    }
}

$argomenti = @(
    '--config-file', $Config,
    'compile',
    '-b', "${Board}:${Opzioni}",
    '--build-property', $FlagExtra,
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
