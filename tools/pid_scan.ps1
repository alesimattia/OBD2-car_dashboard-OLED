<#
    Scansione completa OBD2 e UDS della vettura da PC, tramite un adattatore ELM327 su porta COM (Vgate vLinker FS USB:
    bridge FTDI, 115200 baud di fabbrica). Esegue le stesse fasi della scansione completa del firmware (obd_scan.h),
    con gli stessi servizi in sola lettura e le stesse regole failsafe: centraline OBD e UDS, PID e DID con decodifica
    OBDb, freeze frame, monitor Mode 06, info veicolo, identificazione UDS, DTC e profilo motore suggerito. Non invia
    mai Mode 04/08 nè i servizi UDS che scrivono o cambiano sessione, e non legge il VIN.

    Le tabelle si leggono all'avvio dal firmware, così restano allineate: elenco ODIS, DID di identificazione, nomi dei
    TID e contatori IPT da obd_scan.h, scale UASID e lunghezze dei PID da obd_proto.h, PID del profilo da obd_live.h,
    segnali dalla selezione comune tools\obdb\selection.json, nomi italiani di PID e DTC da pid_descriptions.h e
    dtc_descriptions.h se presenti.

    Il report pid_scan_<data>_<ora>.txt, con nomi, valori e unità, riepilogo per centralina, profilo suggerito e in
    coda la traccia TX/RX, si scrive nella cartella dello script anche dopo un errore o un Ctrl+C.

    Uso, a quadro acceso, auto ferma e con FORScan e VCDS chiusi:
      .\pid_scan.ps1 -Port COM5                     porta del vLinker in Gestione dispositivi
      .\pid_scan.ps1                                se c'è una sola porta COM
      .\pid_scan.ps1 -Port COM5 -BaudRate 115200    baud fisso invece della ricerca
      .\pid_scan.ps1 -Port COM5 -Verbose            mostra a console anche il traffico TX/RX
    Se la policy della macchina blocca gli script locali:
      powershell -ExecutionPolicy Bypass -File .\pid_scan.ps1 -Port COM5

    @Mattia Alesi
#>
[CmdletBinding()]
param(
    [string] $Port,
    [int] $BaudRate = 0
)

# Baud provati in ordine quando -BaudRate non è indicato: 115200 è il valore di fabbrica
# del vLinker FS, gli altri coprono un'unità riconfigurata o un ELM327 originale
$CandidateBaudRates = @(115200, 38400, 230400, 500000)

# Traccia di tutti gli scambi con l'adattatore, riportata in coda al report
$TraceLines = New-Object System.Collections.Generic.List[string]

# Cartella dello script, da cui si trovano header del firmware e selezione OBDb anche quando è caricato con dot-source
$PidScanToolsDirectory = $PSScriptRoot

# Messaggio con cui la regola dei 5 errori di trasporto di fila interrompe la fase corrente
$PhaseSkippedMarker = 'PID_SCAN_PHASE_SKIPPED'

# Invia un comando all'ELM327 e ne raccoglie le righe fino al prompt '>'; ogni riga 7F xx 78 sposta la scadenza di 5 s
# entro il tetto del firmware. Senza prompt scarta per 1 s e interrompe l'ELM con uno spazio, che a riposo ignora, così
# lo STOPPED provocato resta fuori dalle righe restituite e non conta come errore di trasporto.
# @since 29/10/26 Mattia Alesi
function Invoke-ElmCommand($SerialPort, [string] $Command, [int] $TimeoutMs = 3000) {
    $SerialPort.DiscardInBuffer()
    $SerialPort.Write($Command + "`r")
    $received = New-Object System.Text.StringBuilder
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $deadlineMs = [long]$TimeoutMs
    $pendingCapMs = [long](Get-ResponsePendingCapMs)
    $lineBreaks = [char[]]"`r`n"
    $lineStart = 0
    $promptReceived = $false
    while ($stopwatch.ElapsedMilliseconds -lt $deadlineMs) {
        if ($SerialPort.BytesToRead -le 0) {
            Start-Sleep -Milliseconds 5
            continue
        }
        $chunk = [string]$SerialPort.ReadExisting()
        $promptIndex = $chunk.IndexOf('>')
        if ($promptIndex -ge 0) {
            $chunk = $chunk.Substring(0, $promptIndex)
            $promptReceived = $true
        }
        [void]$received.Append($chunk)
        $text = $received.ToString()
        $lineEnd = $text.IndexOfAny($lineBreaks, $lineStart)
        while ($lineEnd -ge 0) {
            if (Test-ElmPendingLine $text.Substring($lineStart, $lineEnd - $lineStart)) {
                $deadlineMs = [math]::Max($deadlineMs, [math]::Min($stopwatch.ElapsedMilliseconds + 5000, $pendingCapMs))
            }
            $lineStart = $lineEnd + 1
            $lineEnd = $text.IndexOfAny($lineBreaks, $lineStart)
        }
        if ($promptReceived) {
            break
        }
    }
    $lines = @(ConvertTo-ElmLines $received.ToString())

    $script:TraceLines.Add("TX $Command")
    Write-Verbose "TX $Command"
    foreach ($line in $lines) {
        $script:TraceLines.Add("RX $line")
        Write-Verbose "RX $line"
    }
    if (-not $promptReceived) {
        $script:TraceLines.Add("-- nessun prompt entro $deadlineMs ms: la risposta si chiude qui")
        $late = Receive-ElmUntilPrompt $SerialPort 1000
        foreach ($line in @(ConvertTo-ElmLines $late.Text)) { $script:TraceLines.Add("RX scartata $line") }
        if (-not $late.Prompt) {
            $SerialPort.Write(' ')
            $script:TraceLines.Add("TX spazio per interrompere l'adattatore")
            $stop = Receive-ElmUntilPrompt $SerialPort 1000
            foreach ($line in @(ConvertTo-ElmLines $stop.Text)) { $script:TraceLines.Add("RX scartata $line") }
        }
    }
    return $lines
}

# Apre la porta dell'adattatore e ne trova il baud inviando ATI finchè la risposta contiene ELM327.
# Senza -Port usa l'unica porta presente; con più porte si ferma, per non inviare comandi a dispositivi sconosciuti.
function Open-ElmPort([string] $PortName, [int] $Baud) {
    if (-not $PortName) {
        $portNames = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object -Unique)
        if ($portNames.Count -eq 0) {
            throw 'Nessuna porta COM presente: collega il vLinker e controlla in Gestione dispositivi che il driver FTDI sia installato.'
        }
        if ($portNames.Count -gt 1) {
            $portDescriptions = @(Get-CimInstance Win32_PnPEntity -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match '\(COM\d+\)' } | ForEach-Object { '  ' + $_.Name })
            throw ("Ci sono più porte COM, indica quella del vLinker con -Port:`n" + ($portDescriptions -join "`n"))
        }
        $PortName = $portNames[0]
    }

    $baudRates = if ($Baud -gt 0) { @($Baud) } else { $script:CandidateBaudRates }
    foreach ($candidateBaud in $baudRates) {
        $serialPort = New-Object System.IO.Ports.SerialPort $PortName, $candidateBaud, 'None', 8, 'One'
        $serialPort.Handshake = 'None'
        # RTS è l'ingresso di interruzione dell'ELM327, attivo basso: resta non asserito, come DTR
        $serialPort.RtsEnable = $false
        $serialPort.DtrEnable = $false
        try {
            $serialPort.Open()
        }
        catch {
            throw "Impossibile aprire $PortName. La porta è occupata da FORScan o VCDS? Dettaglio: $($_.Exception.Message)"
        }
        # Il primo ATI può essere assorbito dai byte spuri ricevuti a un baud sbagliato, quindi si ripete una volta
        for ($attempt = 1; $attempt -le 2; $attempt++) {
            if ((@(Invoke-ElmCommand $serialPort 'ATI' 1500) -join ' ') -match 'ELM327') {
                return $serialPort
            }
        }
        $serialPort.Close()
    }
    throw "Nessuna risposta ELM327 su $PortName ai baud $($baudRates -join ', ')."
}

# Separa ID CAN e byte del frame (PCI compreso) da una riga ELM con header. Accetta le righe con spazi di ATS1,
# con o senza la DLC di AT D1, e quelle compatte delle catture OBDb, dove la parità delle cifre distingue l'ID a
# 11 bit (3 cifre) da quello a 29 bit (8). Con -Extended una riga con spazi porta l'ID a 29 bit in 4 token.
# Ritorna $null per le righe di stato (NO DATA, SEARCHING..., STOPPED).
function ConvertTo-CanFrame([string] $Line, [switch] $Extended) {
    if (-not $Extended -and $Line -match '^[0-9A-F]{3}( [0-9A-F])?( [0-9A-F]{2}){1,8}$') {
        # Il token di una sola cifra dopo l'ID è la DLC, che compare solo se l'adattatore ha AT D1 attivo
        $tokens = @($Line.Split(' ') | Where-Object { $_.Length -ne 1 })
        return @{ Id = $tokens[0]; Bytes = [byte[]]@($tokens | Select-Object -Skip 1 | ForEach-Object { [Convert]::ToByte($_, 16) }) }
    }
    if ($Extended -and $Line -match '^([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2}) ([0-9A-F]{2})( [0-9A-F])?((?: [0-9A-F]{2}){1,8})$') {
        $id = $Matches[1] + $Matches[2] + $Matches[3] + $Matches[4]
        return @{ Id = $id; Bytes = [byte[]]@($Matches[6].Trim().Split(' ') | ForEach-Object { [Convert]::ToByte($_, 16) }) }
    }
    if ($Line -match '^[0-9A-F]+$') {
        $idDigits = if ($Line.Length % 2 -eq 1) { 3 } else { 8 }
        $dataDigits = $Line.Length - $idDigits
        if ($dataDigits -lt 2 -or $dataDigits -gt 16) { return $null }
        $frame = New-Object byte[] ($dataDigits / 2)
        for ($i = 0; $i -lt $frame.Length; $i++) { $frame[$i] = [Convert]::ToByte($Line.Substring($idDigits + 2 * $i, 2), 16) }
        return @{ Id = $Line.Substring(0, $idDigits); Bytes = $frame }
    }
    return $null
}

# Raggruppa per ID CAN le righe stampate con ATH1 e riassembla i messaggi ISO-TP, tenendo tutti i messaggi completi di
# ogni ID; con IncompleteById vi finiscono gli ID con un multi-frame rimasto a metà. La usa la ricerca del protocollo,
# mentre la scansione usa il collettore per centralina che replica il firmware.
function ConvertFrom-CanLines([string[]] $Lines, [switch] $Extended, [hashtable] $IncompleteById) {
    $messagesById = @{}
    $pendingById = @{}
    foreach ($line in $Lines) {
        $frame = ConvertTo-CanFrame $line -Extended:$Extended
        if ($null -eq $frame -or $frame.Bytes.Count -eq 0) {
            continue
        }
        $canId = $frame.Id
        $frameBytes = $frame.Bytes
        $frameType = $frameBytes[0] -shr 4
        $completedMessage = $null

        # In PowerShell 1..0 vale 1,0 e non un insieme vuoto, quindi le lunghezze si controllano prima di estrarre i byte
        if ($frameType -eq 0) {
            $length = $frameBytes[0] -band 0x0F
            if ($length -ge 1 -and $length -le $frameBytes.Count - 1) {
                $completedMessage = [byte[]]$frameBytes[1..$length]
            }
        }
        elseif ($frameType -eq 1 -and $frameBytes.Count -ge 3) {
            $pendingById[$canId] = @{
                Length       = (($frameBytes[0] -band 0x0F) -shl 8) -bor $frameBytes[1]
                NextSequence = 1
                Data         = New-Object System.Collections.Generic.List[byte]
            }
            $pendingById[$canId].Data.AddRange([byte[]]$frameBytes[2..($frameBytes.Count - 1)])
        }
        elseif ($frameType -eq 2 -and $frameBytes.Count -ge 2 -and $pendingById.ContainsKey($canId)) {
            $pending = $pendingById[$canId]
            # Un Consecutive Frame fuori sequenza vuol dire un frame perso, quindi il messaggio non è ricostruibile
            if (($frameBytes[0] -band 0x0F) -ne $pending.NextSequence) {
                $pendingById.Remove($canId)
                continue
            }
            $pending.NextSequence = ($pending.NextSequence + 1) -band 0x0F
            $pending.Data.AddRange([byte[]]$frameBytes[1..($frameBytes.Count - 1)])
            if ($pending.Data.Count -ge $pending.Length) {
                $completedMessage = $pending.Data.GetRange(0, $pending.Length).ToArray()
                $pendingById.Remove($canId)
            }
        }

        if ($null -ne $completedMessage) {
            if (-not $messagesById.ContainsKey($canId)) {
                $messagesById[$canId] = New-Object System.Collections.ArrayList
            }
            [void]$messagesById[$canId].Add($completedMessage)
        }
    }
    if ($null -ne $IncompleteById) {
        foreach ($canId in $pendingById.Keys) { $IncompleteById[$canId] = $true }
    }
    return $messagesById
}

# Estrae il valore grezzo di un segnale con la convenzione OBDb (python/can/signals.py): bit MSB0 a partire da bix,
# contati dal primo byte dopo servizio ed eco; con blsb e più di 8 bit i byte del campo si leggono in ordine
# inverso. Senza dati sufficienti ritorna $null, salvo per le enumerazioni, dove OBDb legge come zero i bit mancanti.
function Get-ObdbRawBits([byte[]] $Data, [int] $BitOffset, [int] $BitLength, [bool] $BytesLsb, [bool] $MissingAsZero) {
    $endBit = $BitOffset + $BitLength
    if (-not $MissingAsZero -and $endBit -gt $Data.Length * 8) { return $null }
    $bytes = [byte[]]$Data.Clone()
    if ($BytesLsb -and $BitLength -gt 8) {
        $startByte = $BitOffset -shr 3
        $endByte = [math]::Min($startByte + (($BitLength + 7) -shr 3), $bytes.Length)
        if ($endByte -gt $startByte) { [array]::Reverse($bytes, $startByte, $endByte - $startByte) }
    }
    [uint64]$result = 0
    for ($bit = $BitOffset; $bit -lt $endBit; $bit++) {
        $byteIndex = $bit -shr 3
        if ($byteIndex -lt $bytes.Length -and (($bytes[$byteIndex] -shr (7 - ($bit % 8))) -band 1)) {
            $result = $result -bor ([uint64]1 -shl ($endBit - $bit - 1))
        }
    }
    return $result
}

# Decodifica un segnale della selezione OBDb da un payload. Valore in double con la formula del decoder ufficiale
# raw × mul / div + add, clamp fra min (0 se assente) e max quando max > min, poi i limiti null con l'epsilon di
# Python; per le enumerazioni il testo e il codice della mappa. Ritorna $null se il dato manca o è nullo.
function ConvertFrom-ObdbSignal($Signal, [byte[]] $Data) {
    $isEnum = $null -ne $Signal.map
    $raw = Get-ObdbRawBits $Data ([int]$Signal.bix) ([int]$Signal.len) ([bool]$Signal.blsb) $isEnum
    if ($null -eq $raw) { return $null }
    if ($isEnum) {
        $entry = $Signal.map | Where-Object { [uint64]$_.raw -eq $raw } | Select-Object -First 1
        if ($null -eq $entry) { return $null }
        return @{ Raw = $raw; Value = $null; Text = $entry.text; Code = $entry.value }
    }
    [double]$number = $raw
    if ($Signal.sign -and ($raw -band ([uint64]1 -shl ([int]$Signal.len - 1)))) { $number -= [math]::Pow(2, [int]$Signal.len) }
    $value = $number * [double]$Signal.mul / [double]$Signal.div + [double]$Signal.add
    $min = if ($null -ne $Signal.min) { [double]$Signal.min } else { 0.0 }
    if ($null -ne $Signal.max -and [double]$Signal.max -gt $min) {
        $value = [math]::Max($min, [math]::Min($value, [double]$Signal.max))
    }
    # sys.float_info.epsilon di Python, non [double]::Epsilon che è il minimo subnormale
    $pythonEpsilon = 2.220446049250313E-16
    if ($null -ne $Signal.nullMin -and $value -le [double]$Signal.nullMin + $pythonEpsilon) { return $null }
    if ($null -ne $Signal.nullMax -and $value -ge [double]$Signal.nullMax - $pythonEpsilon) { return $null }
    return @{ Raw = $raw; Value = $value; Text = $null; Code = $null }
}

# ============================================================
# Righe dell'adattatore
# ============================================================

# Tetto delle attese 7F xx 78 letto da obd_proto.h; prima del caricamento delle tabelle vale quello del firmware.
function Get-ResponsePendingCapMs {
    if ($null -ne $script:ScanTables) { return $script:ScanTables.Constants['OBD_RESPONSE_PENDING_CAP_MS'] }
    return 10000
}

# Divide il testo ricevuto dall'adattatore in righe non vuote, senza i byte 0x00 spuri dell'ELM327 (datasheet p.9).
function ConvertTo-ElmLines([string] $Text) {
    $clean = $Text.Replace([string][char]0, '').Replace('>', '')
    return @($clean -split "[`r`n]+" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' })
}

# Vero se la riga è una risposta 7F xx 78 con header, letta con le cifre di ID del protocollo attivo: la centralina
# chiede altro tempo prima della risposta definitiva.
function Test-ElmPendingLine([string] $Line) {
    $extended = ($null -ne $script:ElmState) -and $script:ElmState.ExtendedProtocol
    $frame = ConvertTo-CanFrame $Line.Replace([string][char]0, '').Trim() -Extended:$extended
    return ($null -ne $frame -and $frame.Bytes.Count -ge 4 -and $frame.Bytes[0] -eq 0x03 -and $frame.Bytes[1] -eq 0x7F -and $frame.Bytes[3] -eq 0x78)
}

# Accumula quello che l'adattatore invia fino al prompt o allo scadere del tempo indicato.
function Receive-ElmUntilPrompt($SerialPort, [int] $TimeoutMs) {
    $received = New-Object System.Text.StringBuilder
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    while ($stopwatch.ElapsedMilliseconds -lt $TimeoutMs) {
        if ($SerialPort.BytesToRead -le 0) {
            Start-Sleep -Milliseconds 5
            continue
        }
        $chunk = [string]$SerialPort.ReadExisting()
        $promptIndex = $chunk.IndexOf('>')
        if ($promptIndex -ge 0) {
            [void]$received.Append($chunk.Substring(0, $promptIndex))
            return @{ Prompt = $true; Text = $received.ToString() }
        }
        [void]$received.Append($chunk)
    }
    return @{ Prompt = $false; Text = $received.ToString() }
}

# ============================================================
# Tabelle del firmware e selezione OBDb
# ============================================================

# Legge un header del firmware dalla cartella del progetto: senza un header obbligatorio le tabelle non sarebbero quelle
# del firmware, quindi lo script si ferma con un errore chiaro; per quelli facoltativi ritorna $null.
function Read-FirmwareHeader([string] $FileName, [bool] $Mandatory) {
    $path = Join-Path (Split-Path $script:PidScanToolsDirectory -Parent) $FileName
    if (Test-Path -LiteralPath $path) { return [IO.File]::ReadAllText($path, [Text.Encoding]::UTF8) }
    if ($Mandatory) { throw "Manca $($path): la scansione legge da questo header le tabelle del firmware e senza non può partire." }
    return $null
}

# Ricostruisce il testo di uno o più letterali C concatenati, con gli escape esadecimali e ottali dei byte UTF-8 (per
# esempio "\xC2\xB0" "C" diventa °C).
function ConvertFrom-CStringLiterals([string] $Source) {
    $bytes = New-Object System.Collections.Generic.List[byte]
    $simpleEscapes = @{ 'n' = 10; 't' = 9; 'r' = 13 }
    foreach ($literal in [regex]::Matches($Source, '"((?:[^"\\]|\\.)*)"')) {
        foreach ($piece in [regex]::Matches($literal.Groups[1].Value, '\\x([0-9A-Fa-f]+)|\\([0-7]{1,3})|\\(.)|([^\\]+)')) {
            if ($piece.Groups[1].Success) { $bytes.Add([byte]([Convert]::ToInt32($piece.Groups[1].Value, 16) -band 0xFF)) }
            elseif ($piece.Groups[2].Success) { $bytes.Add([byte]([Convert]::ToInt32($piece.Groups[2].Value, 8) -band 0xFF)) }
            elseif ($piece.Groups[3].Success) {
                $escaped = $piece.Groups[3].Value
                if ($simpleEscapes.ContainsKey($escaped)) { $bytes.Add([byte]$simpleEscapes[$escaped]) } else { $bytes.Add([byte][char]$escaped) }
            }
            else { $bytes.AddRange([Text.Encoding]::UTF8.GetBytes($piece.Groups[4].Value)) }
        }
    }
    return [Text.Encoding]::UTF8.GetString($bytes.ToArray())
}

# Raccoglie i testi PROGMEM di un header (static const char NOME[] PROGMEM = "...") per nome della costante.
function Get-HeaderStrings([string] $Text) {
    $strings = @{}
    $pattern = 'static\s+const\s+char\s+(\w+)\s*\[\s*\]\s*(?:OBD_PROGMEM|PROGMEM)\s*=\s*((?:"(?:[^"\\]|\\.)*"\s*)+);'
    foreach ($match in [regex]::Matches($Text, $pattern)) {
        $strings[$match.Groups[1].Value] = ConvertFrom-CStringLiterals $match.Groups[2].Value
    }
    return $strings
}

# Restituisce il testo fra le graffe dell'inizializzatore di una tabella dell'header, senza commenti; se la tabella non
# si trova il formato del firmware è cambiato e lo script si ferma invece di usare tabelle sbagliate.
function Get-HeaderArrayBody([string] $Text, [string] $Name, [string] $FileName) {
    $match = [regex]::Match($Text, '\b' + $Name + '\s*\[[^\]]*\]\s*(?:OBD_PROGMEM|PROGMEM)?\s*=\s*\{(.*?)\};', 'Singleline')
    if (-not $match.Success) { throw "Tabella $Name non trovata in $($FileName): il formato del firmware non è più quello letto dallo script." }
    return [regex]::Replace($match.Groups[1].Value, '/\*.*?\*/|//[^\r\n]*', '', 'Singleline')
}

# Legge il valore intero di una costante constexpr dell'header.
function Get-HeaderConstant([string] $Text, [string] $Name, [string] $FileName) {
    $match = [regex]::Match($Text, 'constexpr\s+\w+\s+' + $Name + '\s*=\s*(\d+)\s*;')
    if (-not $match.Success) { throw "Costante $Name non trovata in $($FileName): il formato del firmware non è più quello letto dallo script." }
    return [int]$match.Groups[1].Value
}

# Restituisce il testo PROGMEM indicato da una tabella, fermandosi se il nome non corrisponde a nessun testo.
function Get-RequiredString([hashtable] $Strings, [string] $Name, [string] $FileName) {
    if (-not $Strings.ContainsKey($Name)) { throw "Testo $Name non trovato in $($FileName): il formato del firmware non è più quello letto dallo script." }
    return $Strings[$Name]
}

# Prepara un comando della selezione OBDb per la scansione: identificatore numerico, segnali per id (come l'indice
# locale del generatore), selettore CIM preferendo CIM_SUP_FLAG, byte di dati letti e dipendenza della scala da 4F o 50.
function ConvertTo-ScanCommand($Command) {
    $signalById = New-Object 'System.Collections.Generic.Dictionary[string,object]'
    $maxBits = 0
    $depends4F = $false
    $depends50 = $false
    foreach ($signal in $Command.signals) {
        $signalById[[string]$signal.id] = $signal
        $maxBits = [math]::Max($maxBits, [int]$signal.bix + [int]$signal.len)
        if ($signal.scaleDependsOn -eq '4F') { $depends4F = $true }
        if ($signal.scaleDependsOn -eq '50') { $depends50 = $true }
    }
    $cim = $null
    foreach ($cimId in @('CIM_SUP', 'CIM_SUP_FLAG')) {
        if ($signalById.ContainsKey($cimId)) { $cim = $signalById[$cimId] }
    }
    return @{
        IsSae        = ($Command.source -eq 'SAE')
        IdValue      = [Convert]::ToInt32([string]$Command.id, 16)
        Header       = ([string]$Command.hdr).ToUpperInvariant()
        Response     = ([string]$Command.rax).ToUpperInvariant()
        Captured     = ($Command.capture -eq 'risposto')
        Signals      = @($Command.signals)
        SignalById   = $signalById
        Cim          = $cim
        PayloadBytes = [int][math]::Floor(($maxBits + 7) / 8)
        Depends4F    = $depends4F
        Depends50    = $depends50
    }
}

# Carica le tabelle della scansione dagli header del firmware e dalla selezione OBDb, prima di aprire la porta: così un
# file mancante o cambiato ferma lo script senza aver interrogato la vettura.
function Import-ScanTables {
    $scanText = Read-FirmwareHeader 'obd_scan.h' $true
    $protoText = Read-FirmwareHeader 'obd_proto.h' $true
    $liveText = Read-FirmwareHeader 'obd_live.h' $true
    $pidText = Read-FirmwareHeader 'pid_descriptions.h' $false
    $dtcText = Read-FirmwareHeader 'dtc_descriptions.h' $false
    $selectionPath = Join-Path $script:PidScanToolsDirectory 'obdb\selection.json'
    if (-not (Test-Path -LiteralPath $selectionPath)) { throw "Manca $($selectionPath): si genera con tools\obdb_select.ps1." }
    $selection = [IO.File]::ReadAllText($selectionPath, [Text.Encoding]::UTF8) | ConvertFrom-Json

    $constants = @{}
    foreach ($name in @('OBD_MAX_ECUS', 'OBD_MAX_MESSAGE_LEN', 'OBD_MAX_PIDS_PER_REQUEST', 'OBD_RESPONSE_PENDING_CAP_MS', 'OBD_SCAN_DTC_PER_KIND')) {
        $constants[$name] = Get-HeaderConstant $protoText $name 'obd_proto.h'
    }
    foreach ($name in @('OBD_MAX_UDS_ECUS', 'OBD_SCAN_ECU_BUDGET_MS', 'OBD_SCAN_SILENCE_LIMIT', 'OBD_SCAN_TRANSPORT_ERROR_LIMIT', 'OBD_SCAN_F4_NRC_LIMIT')) {
        $constants[$name] = Get-HeaderConstant $scanText $name 'obd_scan.h'
    }

    $scanStrings = Get-HeaderStrings $scanText
    $odisRows = New-Object System.Collections.Generic.List[object]
    $odisByRequest = @{}
    foreach ($match in [regex]::Matches((Get-HeaderArrayBody $scanText 'OBD_ODIS_ECUS' 'obd_scan.h'), '\{\s*0x([0-9A-Fa-f]{3})\s*,\s*0x([0-9A-Fa-f]{3})\s*,\s*(\w+)\s*\}')) {
        $row = @{
            RequestId  = $match.Groups[1].Value.ToUpperInvariant()
            ResponseId = $match.Groups[2].Value.ToUpperInvariant()
            Name       = (Get-RequiredString $scanStrings $match.Groups[3].Value 'obd_scan.h')
        }
        $odisRows.Add($row)
        # Come obdOdisFindRequest() vale la prima riga con quella richiesta
        if (-not $odisByRequest.ContainsKey($row.RequestId)) { $odisByRequest[$row.RequestId] = $row }
    }
    $identDids = New-Object System.Collections.Generic.List[int]
    foreach ($match in [regex]::Matches((Get-HeaderArrayBody $scanText 'OBD_IDENT_DIDS' 'obd_scan.h'), '0x([0-9A-Fa-f]{4})')) {
        $identDids.Add([Convert]::ToInt32($match.Groups[1].Value, 16))
    }
    $identNames = New-Object System.Collections.Generic.List[string]
    foreach ($match in [regex]::Matches((Get-HeaderArrayBody $scanText 'OBD_IDENT_NAMES' 'obd_scan.h'), '\b(\w+)\b')) {
        $identNames.Add((Get-RequiredString $scanStrings $match.Groups[1].Value 'obd_scan.h'))
    }
    $tidNames = New-Object System.Collections.Generic.List[string]
    foreach ($match in [regex]::Matches((Get-HeaderArrayBody $scanText 'OBD_TID_NAMES' 'obd_scan.h'), '\b(\w+)\b')) {
        $tidNames.Add((Get-RequiredString $scanStrings $match.Groups[1].Value 'obd_scan.h'))
    }
    $probeSet = New-Object System.Collections.Generic.List[int]
    foreach ($match in [regex]::Matches((Get-HeaderArrayBody $scanText 'probeSet' 'obd_scan.h'), '0x([0-9A-Fa-f]{2})')) {
        $probeSet.Add([Convert]::ToInt32($match.Groups[1].Value, 16))
    }
    if ($odisRows.Count -eq 0 -or $identDids.Count -eq 0 -or $identDids.Count -ne $identNames.Count -or $tidNames.Count -ne 12 -or $probeSet.Count -eq 0) {
        throw 'Tabelle ODIS, DID di identificazione, TID o PID F4xx di prova di obd_scan.h in un formato non riconosciuto.'
    }

    $protoStrings = Get-HeaderStrings $protoText
    $uasid = New-Object 'System.Collections.Generic.Dictionary[int,object]'
    $culture = [Globalization.CultureInfo]::InvariantCulture
    $uasidPattern = '\{\s*0x([0-9A-Fa-f]{2})\s*,\s*(true|false)\s*,\s*([-+0-9.eE]+)\s*,\s*([-+0-9.eE]+)\s*,\s*(\w+)\s*\}'
    foreach ($match in [regex]::Matches((Get-HeaderArrayBody $protoText 'OBD_UASID_TABLE' 'obd_proto.h'), $uasidPattern)) {
        $id = [Convert]::ToInt32($match.Groups[1].Value, 16)
        $uasid[$id] = @{
            Id     = $id
            Signed = ($match.Groups[2].Value -eq 'true')
            Scale  = [double]::Parse($match.Groups[3].Value, $culture)
            Offset = [double]::Parse($match.Groups[4].Value, $culture)
            Unit   = (Get-RequiredString $protoStrings $match.Groups[5].Value 'obd_proto.h')
        }
    }
    $lengthTokens = @((Get-HeaderArrayBody $protoText 'OBD_PID_DATA_LEN' 'obd_proto.h') -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' })
    if ($uasid.Count -eq 0 -or $lengthTokens.Count -ne 256) { throw 'Tabelle UASID o lunghezze dei PID di obd_proto.h in un formato non riconosciuto.' }

    # Stesso ordine e stesso limite di 32 di obdLiveProfileCandidates(): prima gli slot live, poi la diagnostica
    $profileCandidates = New-Object System.Collections.Generic.List[int]
    foreach ($tableName in @('OBD_LIVE_SLOT_PID', 'OBD_DIAG_PIDS')) {
        foreach ($match in [regex]::Matches((Get-HeaderArrayBody $liveText $tableName 'obd_live.h'), '0x([0-9A-Fa-f]{2})')) {
            $value = [Convert]::ToInt32($match.Groups[1].Value, 16)
            if (-not $profileCandidates.Contains($value) -and $profileCandidates.Count -lt 32) { $profileCandidates.Add($value) }
        }
    }

    $pidNames = New-Object 'System.Collections.Generic.Dictionary[int,string]'
    if ($null -ne $pidText) {
        foreach ($entry in (Get-HeaderStrings $pidText).GetEnumerator()) {
            if ($entry.Key -match '^PD_S_([0-9A-Fa-f]{2})$') { $pidNames[[Convert]::ToInt32($Matches[1], 16)] = $entry.Value }
        }
    }
    $dtcNames = @{}
    if ($null -ne $dtcText) {
        foreach ($entry in (Get-HeaderStrings $dtcText).GetEnumerator()) {
            if ($entry.Key -match '^dtc_([PCBU][0-9A-F]{4})$') { $dtcNames[$Matches[1].ToUpperInvariant()] = $entry.Value }
        }
    }

    $units = @{}
    foreach ($property in $selection.units.PSObject.Properties) { $units[$property.Name] = [string]$property.Value }
    $booleans = @{}
    foreach ($property in $selection.booleans.PSObject.Properties) { $booleans[$property.Name] = [string[]]@($property.Value) }
    $saeCommands = New-Object System.Collections.Generic.List[object]
    $vagCommands = New-Object System.Collections.Generic.List[object]
    $saeByPid = New-Object 'System.Collections.Generic.Dictionary[int,object]'
    foreach ($command in $selection.commands) {
        $entry = ConvertTo-ScanCommand $command
        if ($entry.IsSae) {
            $saeCommands.Add($entry)
            if (-not $saeByPid.ContainsKey($entry.IdValue)) { $saeByPid[$entry.IdValue] = $entry }
        }
        else {
            $vagCommands.Add($entry)
        }
    }

    $script:ScanTables = @{
        Constants         = $constants
        OdisRows          = $odisRows
        OdisByRequest     = $odisByRequest
        IdentDids         = $identDids
        IdentNames        = $identNames
        TidNames          = $tidNames
        F4ProbeSet        = $probeSet
        IptSpark          = (Get-RequiredString $scanStrings 'OBD_IPT_SPARK' 'obd_scan.h').Split(',')
        IptCompression    = (Get-RequiredString $scanStrings 'OBD_IPT_COMPRESSION' 'obd_scan.h').Split(',')
        Uasid             = $uasid
        PidDataLength     = [int[]]$lengthTokens
        ProfileCandidates = $profileCandidates
        PidNames          = $pidNames
        DtcNames          = $dtcNames
        Units             = $units
        Booleans          = $booleans
        SaeCommands       = $saeCommands
        VagCommands       = $vagCommands
        SaeByPid          = $saeByPid
        ModelYear         = [int]$selection.modelYear
    }
}

# ============================================================
# Stato della scansione e report
# ============================================================

# Prepara lo stato di una nuova scansione: report, orologio, centraline OBD e UDS, esiti delle prove e del profilo.
function Initialize-ScanState {
    $script:ScanState = @{
        Report            = New-Object System.Collections.Generic.List[string]
        Clock             = New-Object System.Diagnostics.Stopwatch
        Started           = $false
        SummaryWritten    = $false
        Stopped           = $false
        TransportErrors   = 0
        ObdEcus           = New-Object System.Collections.Generic.List[object]
        UdsEcus           = New-Object System.Collections.Generic.List[object]
        ConfirmList       = New-Object System.Collections.Generic.List[string]
        FlowControlTest   = 'NotRun'
        RegisterStnPairs  = $false
        MultiPid2         = $false
        MultiPid6         = $false
        ReferenceTorqueNm = $null
        Obd29Responders   = 0
        ObfcmAvailable    = $false
    }
}

# Prepara lo stato dell'adattatore con le capacità date per buone, come elmInit() del firmware: la scansione le smentisce
# man mano che l'adattatore rifiuta un comando.
function Initialize-ElmState($SerialPort) {
    $script:ElmState = @{
        Port                 = $SerialPort
        Protocol11           = '6'
        Protocol29           = '7'
        ExtendedProtocol     = $false
        ReceiveMode          = 'OBD11'
        CurrentHeader        = $null
        Timing               = 'ATAT0'
        TimingAvailable      = $true
        FlowControlHeader    = $null
        FlowControlUserMode  = $false
        LongTimeout          = $false
        PhysicalAvailable    = $true
        FlowControlAvailable = $true
        ExtendedAvailable    = $true
        PendingHandled       = $true
        StnAvailable         = $false
        StnPairsAvailable    = $true
        StnPairs             = @{}
        Collector            = (New-ElmCollector)
        LastStatus           = 'Ok'
        RequestTimeoutMs     = 3000
    }
}

# Scrive una riga del report, che ha il ruolo del Serial del firmware, e la mostra a console mentre la scansione procede.
function Add-ReportLine([string] $Text) {
    $script:ScanState.Report.Add($Text)
    Write-Host $Text
}

# Millisecondi trascorsi dall'inizio della scansione, usati per il budget di ogni centralina UDS.
function Get-ScanMilliseconds {
    return $script:ScanState.Clock.ElapsedMilliseconds
}

# Formatta un numero come obdPrintNumber(): al più 3 decimali senza zeri finali, interi sopra il miliardo, sempre con
# il punto decimale qualunque sia la lingua di Windows.
function Format-ScanNumber([double] $Value) {
    $culture = [Globalization.CultureInfo]::InvariantCulture
    if ([math]::Abs($Value) -ge 1e9) { return [math]::Round($Value, [MidpointRounding]::AwayFromZero).ToString('0', $culture) }
    $text = [math]::Round($Value, 3, [MidpointRounding]::AwayFromZero).ToString('0.000', $culture).TrimEnd('0')
    return $text.TrimEnd('.')
}

# Scrive un DTC SAE nella forma P0301 come decodeDTC() di dtc_descriptions.h.
function Format-DtcCode([int] $Code) {
    $category = 'PCBU'[($Code -shr 14) -band 0x03]
    return '{0}{1}{2:X}{3:X}{4:X}' -f $category, (($Code -shr 12) -band 0x03), (($Code -shr 8) -band 0x0F), (($Code -shr 4) -band 0x0F), ($Code -band 0x0F)
}

# Testo sì/no dei flag nel report.
function Format-YesNo([bool] $Value) {
    if ($Value) { return 'sì' }
    return 'no'
}

# Copia una parte di un array di byte senza i tranelli degli intervalli di PowerShell, dove 3..2 non è vuoto.
function Get-ByteSlice([byte[]] $Bytes, [int] $Offset, [int] $Count = -1) {
    if ($null -eq $Bytes) { $Bytes = [byte[]]@() }
    if ($Count -lt 0) { $Count = $Bytes.Length - $Offset }
    $Count = [math]::Min($Count, $Bytes.Length - $Offset)
    if ($Count -le 0) { return , ([byte[]]@()) }
    $slice = New-Object byte[] $Count
    [Array]::Copy($Bytes, $Offset, $slice, 0, $Count)
    return , $slice
}

# Nome di una centralina OBD dal suo ID di risposta, come obdEcuDisplayName(): il nome ODIS della richiesta fisica,
# altrimenti "Centralina N" numerata dalla posizione dell'ID.
function Get-ObdEcuDisplayName([string] $EcuId) {
    $value = [Convert]::ToInt32($EcuId, 16)
    if (Test-ObdResponseId $EcuId) {
        $odis = $script:ScanTables.OdisByRequest['{0:X3}' -f ($value - 8)]
        if ($null -ne $odis) { return $odis.Name }
    }
    return 'Centralina {0}' -f ($value - 0x7E8 + 1)
}

# Intestazione di riga di una centralina OBD: "  [Motore 7E8] ".
function Get-ObdTag($Ecu) {
    return '  [{0} {1}] ' -f (Get-ObdEcuDisplayName $Ecu.Id), $Ecu.Id
}

# Nome di una centralina UDS come obdUdsDisplayName(): nome ODIS, altrimenti il nome di sistema F197, altrimenti
# "Centralina senza nome N".
function Get-UdsDisplayName($Ecu) {
    if ($null -ne $Ecu.Odis) { return $Ecu.Odis.Name }
    if ($Ecu.SystemName) { return $Ecu.SystemName }
    return 'Centralina senza nome {0}' -f $Ecu.Number
}

# Intestazione di riga di una centralina UDS: "  [Gateway 710/77A] ".
function Get-UdsTag($Ecu) {
    return '  [{0} {1}/{2}] ' -f (Get-UdsDisplayName $Ecu), $Ecu.RequestId, $Ecu.ResponseId
}

# ============================================================
# Bitmap, risposte e decodifiche fuori dalla selezione OBDb (porting di obd_proto.h)
# ============================================================

# Vero per gli ID a 11 bit delle risposte OBD 7E8-7EF.
function Test-ObdResponseId([string] $CanId) {
    if ($CanId.Length -ne 3) { return $false }
    $value = [Convert]::ToInt32($CanId, 16)
    return $value -ge 0x7E8 -and $value -le 0x7EF
}

# Vero per gli indirizzi funzionali OBD a cui rispondono più centraline (7DF e 18DB33F1).
function Test-FunctionalId([string] $CanId) {
    return $CanId -eq '7DF' -or $CanId -eq '18DB33F1'
}

# Vero per gli ID che riportano una bitmap di supporto (00, 20, ..., E0) invece di un dato.
function Test-RangeId([int] $Id) {
    return ($Id -band 0x1F) -eq 0
}

# Legge un bit di una bitmap di supporto da 32 byte: il bit 7 del primo byte è l'ID 01, e l'ID 0 vale sempre.
function Test-BitmapId([byte[]] $Bitmap, [int] $Id) {
    if ($Id -eq 0) { return $true }
    $index = $Id - 1
    return ($Bitmap[$index -shr 3] -band (0x80 -shr ($index -band 7))) -ne 0
}

# Imposta un bit della bitmap con la numerazione di Test-BitmapId.
function Set-BitmapId([byte[]] $Bitmap, [int] $Id) {
    if ($Id -eq 0) { return }
    $index = $Id - 1
    $Bitmap[$index -shr 3] = [byte]($Bitmap[$index -shr 3] -bor (0x80 -shr ($index -band 7)))
}

# Copia i 4 byte di una risposta di supporto nella posizione del range base.
function Set-BitmapRange([byte[]] $Bitmap, [int] $Base, [byte[]] $Four) {
    [Array]::Copy($Four, 0, $Bitmap, $Base -shr 3, 4)
}

# Conta gli ID dichiarati fra 01 e FF, esclusi quelli che annunciano solo il range successivo.
function Get-DataIdCount([byte[]] $Bitmap) {
    $count = 0
    for ($id = 1; $id -le 0xFF; $id++) {
        if (-not (Test-RangeId $id) -and (Test-BitmapId $Bitmap $id)) { $count++ }
    }
    return $count
}

# Estrae i 4 byte di bitmap da una risposta di supporto come obdParseSupportedRange(): il Mode 02 porta il numero del
# freeze frame prima della bitmap e il Mode 09 può avere il byte NODI. Ritorna $null se la risposta non è quella attesa.
function Get-SupportedRange([byte[]] $Message, [int] $ResponseService, [int] $Base) {
    $offset = if ($ResponseService -eq 0x42 -or ($Message.Length -ge 7 -and $ResponseService -eq 0x49)) { 3 } else { 2 }
    if ($Message.Length -lt $offset + 4 -or $Message[0] -ne $ResponseService -or $Message[1] -ne $Base) { return $null }
    return , (Get-ByteSlice $Message $offset 4)
}

# Verifica una risposta Mode 01 multi-PID con le lunghezze SAE di obd_proto.h, come obdParseMultiPid(): quanti PID
# contiene e se è ben formata, cioè senza PID di lunghezza ignota nè byte avanzati o mancanti.
function Get-MultiPidCheck([byte[]] $Message) {
    $result = @{ FieldCount = 0; WellFormed = $false }
    if ($Message.Length -lt 2 -or $Message[0] -ne 0x41) { return $result }
    $position = 1
    while ($position -lt $Message.Length -and $result.FieldCount -lt $script:ScanTables.Constants['OBD_MAX_PIDS_PER_REQUEST']) {
        $dataLength = $script:ScanTables.PidDataLength[$Message[$position]]
        if ($dataLength -eq 0 -or $position + 1 + $dataLength -gt $Message.Length) { return $result }
        $result.FieldCount++
        $position += 1 + $dataLength
    }
    $result.WellFormed = $position -eq $Message.Length
    return $result
}

# Estrae i DTC da una risposta Mode 03/07/0A come obdParseDtcs(): conteggio dopo il servizio, due byte per codice,
# senza i codici 0000 di riempimento e al più quanti ne conserva il firmware per tipo.
function Get-ObdDtcCodes([byte[]] $Message) {
    $codes = New-Object System.Collections.Generic.List[int]
    if ($Message.Length -lt 2 -or @(0x43, 0x47, 0x4A) -notcontains $Message[0]) { return , $codes }
    $limit = [math]::Min([int]$Message[1], [math]::Floor(($Message.Length - 2) / 2))
    for ($i = 0; $i -lt $limit -and $codes.Count -lt $script:ScanTables.Constants['OBD_SCAN_DTC_PER_KIND']; $i++) {
        $code = ([int]$Message[2 + 2 * $i] -shl 8) -bor $Message[3 + 2 * $i]
        if ($code -ne 0) { $codes.Add($code) }
    }
    return , $codes
}

# Legge il numero di DTC da una risposta positiva 59 01 come obdParseDtcCount(); $null se la risposta non lo contiene.
function Get-UdsDtcCount([byte[]] $Message) {
    if ($Message.Length -lt 6 -or $Message[0] -ne 0x59 -or $Message[1] -ne 0x01) { return $null }
    return ([int]$Message[4] -shl 8) -bor $Message[5]
}

# Copia un contenuto come testo se è stampabile, con il riempimento finale di zeri, spazi o FF tolto, come
# obdCopyPrintableText(); un contenuto binario ritorna $null e non si mostra. MaxChars è la capienza del buffer del firmware.
function ConvertFrom-PrintableText([byte[]] $Data, [int] $MaxChars) {
    $end = $Data.Length
    while ($end -gt 0 -and ($Data[$end - 1] -eq 0x00 -or $Data[$end - 1] -eq 0x20 -or $Data[$end - 1] -eq 0xFF)) { $end-- }
    if ($end -eq 0) { return $null }
    $builder = New-Object System.Text.StringBuilder
    for ($i = 0; $i -lt $end; $i++) {
        if ($Data[$i] -lt 0x20 -or $Data[$i] -gt 0x7E) { return $null }
        if ($builder.Length -lt $MaxChars) { [void]$builder.Append([char]$Data[$i]) }
    }
    return $builder.ToString()
}

# Scrive come AAAA-MM-GG una data BCD di identificazione UDS su 3 o 4 byte, come obdDecodeBcdDate(); $null se i byte
# non sono BCD o mese e giorno non sono plausibili.
function ConvertFrom-BcdDate([byte[]] $Data) {
    if ($Data.Length -ne 3 -and $Data.Length -ne 4) { return $null }
    foreach ($value in $Data) {
        if (($value -shr 4) -gt 9 -or ($value -band 0x0F) -gt 9) { return $null }
    }
    $decimal = @($Data | ForEach-Object { ($_ -shr 4) * 10 + ($_ -band 0x0F) })
    $year = if ($Data.Length -eq 4) { $decimal[0] * 100 + $decimal[1] } else { 2000 + $decimal[0] }
    $month = $decimal[$Data.Length - 2]
    $day = $decimal[$Data.Length - 1]
    if ($month -lt 1 -or $month -gt 12 -or $day -lt 1 -or $day -gt 31) { return $null }
    return '{0:D4}-{1:D2}-{2:D2}' -f $year, $month, $day
}

# Estrae il nome della centralina da [49 0A NODI + 20 byte ASCII] come obdParseEcuName(): solo i caratteri stampabili,
# al più 20; $null se non ne resta nessuno.
function Get-EcuNameText([byte[]] $Message) {
    if ($Message.Length -lt 4 -or $Message[0] -ne 0x49 -or $Message[1] -ne 0x0A) { return $null }
    $builder = New-Object System.Text.StringBuilder
    for ($i = 3; $i -lt $Message.Length -and $builder.Length -lt 20; $i++) {
        if ($Message[$i] -ge 0x20 -and $Message[$i] -le 0x7E) { [void]$builder.Append([char]$Message[$i]) }
    }
    if ($builder.Length -eq 0) { return $null }
    return $builder.ToString()
}

# Legge un intero a 32 bit big-endian senza passare dagli Int32 con segno di PowerShell.
function Get-UInt32BigEndian([byte[]] $Bytes, [int] $Offset) {
    return [uint32](([uint64]$Bytes[$Offset] -shl 24) -bor ([uint64]$Bytes[$Offset + 1] -shl 16) -bor ([uint64]$Bytes[$Offset + 2] -shl 8) -bor [uint64]$Bytes[$Offset + 3])
}

# Decodifica i contatori OBFCM da [49 17 NODI + 16 byte] come obdParseObfcm(), in float come il firmware: distanza e
# carburante di vita, plausibili solo con un consumo medio fra 3 e 30 L/100 km. $null se la risposta è troppo corta.
function ConvertFrom-Obfcm([byte[]] $Message) {
    if ($Message.Length -lt 19 -or $Message[0] -ne 0x49 -or $Message[1] -ne 0x17) { return $null }
    $lifetimeDistanceRaw = Get-UInt32BigEndian $Message 7
    $lifetimeFuelRaw = Get-UInt32BigEndian $Message 15
    $distance = [single]([single]$lifetimeDistanceRaw / [single]10.0)
    $fuel = [single]([single]$lifetimeFuelRaw / [single]100.0)
    $result = @{ Plausible = $false; LifetimeDistanceKm = [double]$distance; LifetimeFuelL = [double]$fuel; AverageLitersPer100 = 0.0 }
    if ($lifetimeDistanceRaw -ne [uint32]::MaxValue -and $lifetimeFuelRaw -ne [uint32]::MaxValue -and $distance -gt 1.0) {
        $litersPer100 = [single]([single]($fuel / $distance) * [single]100.0)
        $result.Plausible = $litersPer100 -ge 3.0 -and $litersPer100 -le 30.0
        $result.AverageLitersPer100 = [double]([single]($fuel / $distance)) * 100.0
    }
    return $result
}

# Nome italiano di un monitor Mode 06 (MID) come obdMode06MonitorName(); $null se lo standard non lo nomina.
function Get-Mode06MonitorName([int] $Mid) {
    if ($Mid -ge 0x01 -and $Mid -le 0x10) { return 'Sonda lambda banco {0} sensore {1}' -f ([int][math]::Floor(($Mid - 0x01) / 4) + 1), (($Mid - 0x01) % 4 + 1) }
    if ($Mid -ge 0x41 -and $Mid -le 0x50) { return 'Riscaldatore sonda lambda banco {0} sensore {1}' -f ([int][math]::Floor(($Mid - 0x41) / 4) + 1), (($Mid - 0x41) % 4 + 1) }
    if ($Mid -ge 0x21 -and $Mid -le 0x24) { return 'Catalizzatore banco {0}' -f ($Mid - 0x20) }
    if ($Mid -ge 0x31 -and $Mid -le 0x34) { return 'EGR banco {0}' -f ($Mid - 0x30) }
    if ($Mid -ge 0x35 -and $Mid -le 0x38) { return 'Fasatura variabile banco {0}' -f ($Mid - 0x34) }
    if ($Mid -eq 0x39) { return 'Evaporazione, tappo aperto (0.150 in)' }
    if ($Mid -eq 0x3A) { return 'Evaporazione (0.090 in)' }
    if ($Mid -eq 0x3B) { return 'Evaporazione (0.040 in)' }
    if ($Mid -eq 0x3C) { return 'Evaporazione (0.020 in)' }
    if ($Mid -eq 0x3D) { return 'Flusso di spurgo evaporazione' }
    if ($Mid -ge 0x61 -and $Mid -le 0x64) { return 'Catalizzatore riscaldato banco {0}' -f ($Mid - 0x60) }
    if ($Mid -ge 0x71 -and $Mid -le 0x74) { return 'Aria secondaria {0}' -f ($Mid - 0x70) }
    if ($Mid -ge 0x81 -and $Mid -le 0x84) { return 'Sistema carburante banco {0}' -f ($Mid - 0x80) }
    if ($Mid -ge 0x85 -and $Mid -le 0x86) { return 'Controllo pressione di sovralimentazione banco {0}' -f ($Mid - 0x84) }
    if ($Mid -ge 0x90 -and $Mid -le 0x91) { return 'Assorbitore NOx banco {0}' -f ($Mid - 0x8F) }
    if ($Mid -ge 0x98 -and $Mid -le 0x99) { return 'Catalizzatore NOx banco {0}' -f ($Mid - 0x97) }
    if ($Mid -eq 0xA1) { return 'Mancate accensioni, dati generali' }
    if ($Mid -ge 0xA2 -and $Mid -le 0xAD) { return 'Mancate accensioni cilindro {0}' -f ($Mid - 0xA1) }
    if ($Mid -ge 0xB0 -and $Mid -le 0xB1) { return 'Filtro antiparticolato banco {0}' -f ($Mid - 0xAF) }
    return $null
}

# Separa una risposta Mode 06 [46 (MID TID UASID valore min max)...] nei test da 9 byte come obdParseMode06(), al più
# 8; un blocco incompleto in coda si scarta.
function Get-Mode06Tests([byte[]] $Message) {
    $tests = New-Object System.Collections.Generic.List[object]
    if ($Message.Length -lt 1 -or $Message[0] -ne 0x46) { return , $tests }
    for ($position = 1; $position + 9 -le $Message.Length -and $tests.Count -lt 8; $position += 9) {
        $tests.Add(@{
                Mid   = [int]$Message[$position]
                Tid   = [int]$Message[$position + 1]
                Uasid = [int]$Message[$position + 2]
                Value = ([int]$Message[$position + 3] -shl 8) -bor $Message[$position + 4]
                Min   = ([int]$Message[$position + 5] -shl 8) -bor $Message[$position + 6]
                Max   = ([int]$Message[$position + 7] -shl 8) -bor $Message[$position + 8]
            })
    }
    return , $tests
}

# Scala UASID di obd_proto.h per l'identificativo indicato; $null se lo standard non la definisce.
function Get-UasidEntry([int] $Uasid) {
    if ($script:ScanTables.Uasid.ContainsKey($Uasid)) { return $script:ScanTables.Uasid[$Uasid] }
    return $null
}

# Applica la scala UASID a un valore a 16 bit come obdUasidValue(): complemento a due per gli UASID con segno,
# vero/falso per 2E.
function Get-UasidValue($Uas, [int] $Raw) {
    if ($Uas.Id -eq 0x2E) { return [double]($Raw -ne 0) }
    $number = if ($Uas.Signed -and $Raw -ge 0x8000) { $Raw - 0x10000 } else { $Raw }
    return [double]$number * $Uas.Scale + $Uas.Offset
}

# Esito di un test Mode 06 come obdMode06Passed(): 1 superato, 0 fallito, -1 con UASID non definito.
function Get-Mode06Outcome($Test) {
    $uas = Get-UasidEntry $Test.Uasid
    if ($null -eq $uas) { return -1 }
    $value = Get-UasidValue $uas $Test.Value
    if ($value -ge (Get-UasidValue $uas $Test.Min) -and $value -le (Get-UasidValue $uas $Test.Max)) { return 1 }
    return 0
}

# ============================================================
# Collettore ISO-TP per centralina (come obdIsoTpFeed e ObdCollector del firmware)
# ============================================================

# Raccolta vuota delle risposte a una richiesta, una slot per ID CAN.
function New-ElmCollector {
    return @{ Slots = (New-Object System.Collections.Generic.List[object]) }
}

# Azzera lo stato di ricezione di una slot.
function Reset-IsoTpSlot($Slot) {
    $Slot.ExpectedLength = 0
    $Slot.TotalReceived = 0
    $Slot.NextSequence = 0
    $Slot.State = 'Idle'
    $Slot.Truncated = $false
    $Slot.Data = New-Object System.Collections.Generic.List[byte]
}

# Restituisce la slot dell'ID CAN indicato, creandola se serve; $null quando le slot del firmware sono finite.
function Get-CollectorSlot([string] $CanId) {
    $slots = $script:ElmState.Collector.Slots
    foreach ($slot in $slots) {
        if ($slot.Id -eq $CanId) { return $slot }
    }
    if ($slots.Count -ge $script:ScanTables.Constants['OBD_MAX_ECUS']) { return $null }
    $slot = @{ Id = $CanId }
    Reset-IsoTpSlot $slot
    $slots.Add($slot)
    return $slot
}

# Accoda byte al messaggio entro la capacità del firmware (OBD_MAX_MESSAGE_LEN): l'eccedenza si conta soltanto, così
# le decodifiche lavorano sugli stessi byte del firmware.
function Add-IsoTpData($Slot, [byte[]] $Frame, [int] $Offset, [int] $Count) {
    $capacity = $script:ScanTables.Constants['OBD_MAX_MESSAGE_LEN']
    for ($i = 0; $i -lt $Count; $i++) {
        if ($Slot.Data.Count -lt $capacity) { $Slot.Data.Add($Frame[$Offset + $i]) } else { $Slot.Truncated = $true }
        $Slot.TotalReceived++
    }
}

# Elabora un frame CAN (PCI compreso) di una centralina come obdIsoTpFeed(): single frame, first frame e consecutive
# frame, con un frame fuori sequenza che porta la slot in errore; il riempimento finale si ignora.
function Add-IsoTpFrame($Slot, [byte[]] $Frame) {
    if ($Frame.Length -eq 0) { return }
    $type = $Frame[0] -shr 4
    if ($type -eq 0) {
        $length = $Frame[0] -band 0x0F
        if ($length -eq 0 -or $length -gt 7 -or $length + 1 -gt $Frame.Length) { return }
        Reset-IsoTpSlot $Slot
        $Slot.ExpectedLength = $length
        Add-IsoTpData $Slot $Frame 1 $length
        $Slot.State = 'Complete'
    }
    elseif ($type -eq 1) {
        if ($Frame.Length -lt 3) { return }
        $total = (($Frame[0] -band 0x0F) -shl 8) -bor $Frame[1]
        if ($total -lt 8) { return }
        Reset-IsoTpSlot $Slot
        $Slot.ExpectedLength = $total
        Add-IsoTpData $Slot $Frame 2 ($Frame.Length - 2)
        $Slot.NextSequence = 1
        $Slot.State = 'Receiving'
    }
    elseif ($type -eq 2) {
        if ($Slot.State -ne 'Receiving') { return }
        if (($Frame[0] -band 0x0F) -ne $Slot.NextSequence) {
            # Frame perso o fuori ordine: il messaggio non è ricostruibile e resta incompleto
            $Slot.State = 'Error'
            return
        }
        $remaining = $Slot.ExpectedLength - $Slot.TotalReceived
        Add-IsoTpData $Slot $Frame 1 ([math]::Min($Frame.Length - 1, $remaining))
        $Slot.NextSequence = ($Slot.NextSequence + 1) -band 0x0F
        if ($Slot.TotalReceived -ge $Slot.ExpectedLength) { $Slot.State = 'Complete' }
    }
}

# Restituisce la slot con il messaggio completo dell'ID indicato, $null se assente o incompleto.
function Get-CompleteMessage([string] $CanId) {
    foreach ($slot in $script:ElmState.Collector.Slots) {
        if ($slot.Id -eq $CanId -and $slot.State -eq 'Complete') { return $slot }
    }
    return $null
}

# Restituisce i byte del messaggio completo dell'ID indicato, $null se assente o incompleto.
function Get-CompleteMessageData([string] $CanId) {
    $slot = Get-CompleteMessage $CanId
    if ($null -eq $slot) { return $null }
    return , $slot.Data.ToArray()
}

# Conta i messaggi completi della raccolta, il valore di ritorno di obdTransact().
function Get-CompleteMessageCount {
    return @($script:ElmState.Collector.Slots | Where-Object { $_.State -eq 'Complete' }).Count
}

# ============================================================
# Trasporto ELM327 (porting di obdTransact e dei comandi AT di WIFI_conn.ino)
# ============================================================

# Invia un comando AT e verifica che l'adattatore risponda OK; un rifiuto finisce nel report come nel firmware.
function Invoke-ElmCommandOk([string] $Command) {
    $ok = (@(Invoke-ElmCommand $script:ElmState.Port $Command) -join ' ') -cmatch 'OK'
    if (-not $ok) { Add-ReportLine "ELM327: comando non accettato $Command" }
    return $ok
}

# Configura l'adattatore per il modo di ricezione come elmApplyReceiveMode(): protocollo a 11 o 29 bit (quello trovato
# all'avvio e il suo gemello), filtri sulle risposte UDS oppure ricezione automatica, STmin 5 ms nei modi UDS. Dopo
# ATSP header e flow control vanno reimpostati; ritorna $false se il 29 bit o i filtri non sono accettati.
function Set-ElmReceiveMode([string] $Mode) {
    $elm = $script:ElmState
    if ($Mode -eq $elm.ReceiveMode) { return $true }
    $extended = $Mode -eq 'OBD29' -or $Mode -eq 'UDS29'
    if ($extended -ne $elm.ExtendedProtocol) {
        if ($extended -and -not $elm.ExtendedAvailable) { return $false }
        $protocol = if ($extended) { $elm.Protocol29 } else { $elm.Protocol11 }
        if (-not (Invoke-ElmCommandOk ('ATSP' + $protocol))) {
            if ($extended) { $elm.ExtendedAvailable = $false }
            return $false
        }
        $elm.ExtendedProtocol = $extended
        $elm.CurrentHeader = $null
        $elm.FlowControlHeader = $null
    }
    $uds = $Mode -eq 'UDS11' -or $Mode -eq 'UDS29'
    if ($Mode -eq 'UDS11') { $ok = (Invoke-ElmCommandOk 'ATCF700') -and (Invoke-ElmCommandOk 'ATCM700') }
    elseif ($Mode -eq 'UDS29') { $ok = (Invoke-ElmCommandOk 'ATCF17FE0000') -and (Invoke-ElmCommandOk 'ATCM1FFFFF00') }
    else { $ok = Invoke-ElmCommandOk 'ATAR' }
    if ($uds) {
        if ($elm.FlowControlAvailable -and -not (Invoke-ElmCommandOk 'ATFCSD300005')) { $elm.FlowControlAvailable = $false }
    }
    elseif ($elm.FlowControlUserMode) {
        $null = Invoke-ElmCommandOk 'ATFCSM0'
        $elm.FlowControlUserMode = $false
        $elm.FlowControlHeader = $null
    }
    # Con un filtro rifiutato il modo resta da applicare, così la richiesta successiva lo ritenta invece di lavorare senza filtri
    $elm.ReceiveMode = if ($ok) { $Mode } else { $null }
    return $ok
}

# Imposta l'header delle richieste solo se diverso dal corrente, come elmSetHeader(): a 11 bit ATSH con 3 cifre, a 29
# bit ATCP con i 5 bit di priorità e ATSH con i 24 bit bassi (17FC007B diventa ATCP17 e ATSHFC007B).
function Set-ElmHeader([string] $RequestId) {
    $elm = $script:ElmState
    if ($RequestId -eq $elm.CurrentHeader) { return $true }
    if ($RequestId.Length -eq 8) {
        $priority = [Convert]::ToInt32($RequestId.Substring(0, 2), 16) -band 0x1F
        if (-not (Invoke-ElmCommandOk ('ATCP{0:X2}' -f $priority))) { return $false }
        $command = 'ATSH' + $RequestId.Substring(2)
    }
    else {
        $command = 'ATSH' + $RequestId
    }
    if (-not (Invoke-ElmCommandOk $command)) { return $false }
    $elm.CurrentHeader = $RequestId
    return $true
}

# Sceglie la temporizzazione: ATAT0 per le richieste funzionali, che devono attendere tutte le centraline, ATAT1 per
# quelle fisiche, che così non pagano ogni volta l'intera attesa. Un rifiuto lascia la temporizzazione com'è.
function Set-ElmTiming([bool] $Functional) {
    $elm = $script:ElmState
    $wanted = if ($Functional) { 'ATAT0' } else { 'ATAT1' }
    if (-not $elm.TimingAvailable -or $elm.Timing -eq $wanted) { return }
    if (Invoke-ElmCommandOk $wanted) { $elm.Timing = $wanted } else { $elm.TimingAvailable = $false }
}

# Indirizza il flow control all'ID di richiesta come vogliono le coppie UDS VAG, come elmSetFlowControlHeader(): in modo
# 0 l'ELM lo manderebbe a risposta - 8. ATFCSH e ATFCSD vanno dati prima di ATFCSM1, altrimenti l'ELM risponde '?'.
function Set-ElmFlowControl([string] $RequestId, [string] $ResponseId) {
    $elm = $script:ElmState
    if (-not $elm.FlowControlAvailable) { return $false }
    if ($RequestId -ne $elm.FlowControlHeader -or -not $elm.FlowControlUserMode) {
        if (-not (Invoke-ElmCommandOk ('ATFCSH' + $RequestId)) -or (-not $elm.FlowControlUserMode -and -not (Invoke-ElmCommandOk 'ATFCSM1'))) {
            $elm.FlowControlAvailable = $false
            return $false
        }
        $elm.FlowControlUserMode = $true
        $elm.FlowControlHeader = $RequestId
    }
    if ($script:ScanState.RegisterStnPairs -and $ResponseId) { Register-StnFlowControlPair $RequestId $ResponseId }
    return $true
}

# Sugli adattatori STN registra la coppia richiesta/risposta, perchè gli STN mandano il flow control solo alle coppie
# registrate; prova STCFCPA e, se rifiutato, la forma precedente STCAFCP. È un comando dell'adattatore, non va all'auto.
function Register-StnFlowControlPair([string] $RequestId, [string] $ResponseId) {
    $elm = $script:ElmState
    $pair = "$RequestId/$ResponseId"
    if (-not $elm.StnAvailable -or -not $elm.StnPairsAvailable -or $elm.StnPairs.ContainsKey($pair)) { return }
    foreach ($command in @("STCFCPA $RequestId, $ResponseId", "STCAFCP $RequestId, $ResponseId")) {
        if ((@(Invoke-ElmCommand $elm.Port $command) -join ' ') -cmatch 'OK') {
            $elm.StnPairs[$pair] = $true
            return
        }
    }
    $elm.StnPairsAvailable = $false
    Add-ReportLine 'Adattatore STN: STCFCPA e STCAFCP rifiutati, il flow control resta affidato a ATFCSH'
}

# Ultima difesa contro i servizi vietati: passano solo le richieste di lettura della scansione (OBD 01 02 03 06 07 09 0A,
# UDS 22, 19 01 e 3E 00) e mai il VIN (09 02 e 22 F190), qualunque sia il chiamante.
function Assert-ReadOnlyRequest([byte[]] $Request) {
    $service = [int]$Request[0]
    $allowed = @(0x01, 0x02, 0x03, 0x06, 0x07, 0x09, 0x0A, 0x19, 0x22, 0x3E)
    $blocked = $allowed -notcontains $service
    if ($service -eq 0x19 -and ($Request.Length -lt 2 -or $Request[1] -ne 0x01)) { $blocked = $true }
    if ($service -eq 0x3E -and ($Request.Length -lt 2 -or $Request[1] -ne 0x00)) { $blocked = $true }
    if ($service -eq 0x09 -and $Request.Length -ge 2 -and $Request[1] -eq 0x02) { $blocked = $true }
    for ($i = 1; $service -eq 0x22 -and $i + 1 -lt $Request.Length; $i += 2) {
        if ($Request[$i] -eq 0xF1 -and $Request[$i + 1] -eq 0x90) { $blocked = $true }
    }
    if ($blocked) {
        throw ('Richiesta {0} bloccata: la scansione usa solo servizi di lettura e non legge il VIN.' -f (($Request | ForEach-Object { '{0:X2}' -f $_ }) -join ' '))
    }
}

# Vero per gli ID che il modo di ricezione accetta come risposta, come obdAcceptsResponse(): in OBD 11 bit 7E8-7EF, in
# UDS 11 bit ogni 700-7FF diverso dalla richiesta, a 29 bit le risposte OBD 18DAF1xx o quelle VAG 17FE00xx.
function Test-AcceptedResponse([string] $Mode, [string] $RequestId, [string] $CanId) {
    $extended = $CanId.Length -eq 8
    $value = [Convert]::ToInt64($CanId, 16)
    switch ($Mode) {
        'UDS11' { return (-not $extended -and $value -ge 0x700 -and $value -le 0x7FF -and $CanId -ne $RequestId) }
        'OBD29' { return ($extended -and ($value -band 0x1FFFFF00) -eq 0x18DAF100) }
        'UDS29' { return ($extended -and ($value -band 0x1FFFFF00) -eq 0x17FE0000) }
    }
    return (-not $extended -and $value -ge 0x7E8 -and $value -le 0x7EF)
}

# Invia la richiesta già impostata e riassembla le righe per centralina, come elmExchange(): una risposta 7F xx 78 azzera
# la slot in attesa di quella definitiva. Ritorna $true se una centralina ha chiesto altro tempo senza poi rispondere.
function Invoke-ElmExchange([string] $Command, [string] $RequestId, [string] $Mode) {
    $elm = $script:ElmState
    $lines = @(Invoke-ElmCommand $elm.Port $Command $elm.RequestTimeoutMs)
    if (($lines -join "`n") -match 'CAN ERROR|BUFFER FULL|STOPPED|\?') { $elm.LastStatus = 'Error' }
    $pendingIds = New-Object System.Collections.Generic.List[string]
    foreach ($line in $lines) {
        $frame = ConvertTo-CanFrame $line -Extended:$elm.ExtendedProtocol
        if ($null -eq $frame) { continue }
        $canId = ([string]$frame.Id).ToUpperInvariant()
        if (-not (Test-AcceptedResponse $Mode $RequestId $canId)) { continue }
        $slot = Get-CollectorSlot $canId
        if ($null -eq $slot) { continue }
        Add-IsoTpFrame $slot $frame.Bytes
        if ($slot.State -eq 'Complete' -and $slot.Data.Count -ge 3 -and $slot.Data[0] -eq 0x7F -and $slot.Data[2] -eq 0x78) {
            Reset-IsoTpSlot $slot
            if (-not $pendingIds.Contains($canId)) { $pendingIds.Add($canId) }
        }
    }
    foreach ($canId in $pendingIds) {
        if ($null -eq (Get-CompleteMessage $canId)) { return $true }
    }
    return $false
}

# Trasporto della scansione, porting di obdTransact() per l'ELM327: modo di ricezione, temporizzazione, header e, nei
# modi UDS, flow control verso l'ID di richiesta; poi invio e riassemblaggio. Se una centralina chiede altro tempo e
# l'adattatore chiude senza risposta, attiva ATSTFF e ripete la richiesta una volta. Ritorna i messaggi completi.
function Invoke-ObdTransact([string] $RequestId, [byte[]] $Request, [string] $WaitForId, [string] $Mode) {
    $elm = $script:ElmState
    $elm.Collector = New-ElmCollector
    $elm.LastStatus = 'Ok'
    if ($null -eq $Request -or $Request.Length -eq 0 -or $Request.Length -gt 7) {
        $elm.LastStatus = 'Error'
        return 0
    }
    Assert-ReadOnlyRequest $Request
    $uds = $Mode -eq 'UDS11' -or $Mode -eq 'UDS29'
    $functional = Test-FunctionalId $RequestId
    if (-not $functional -and -not $elm.PhysicalAvailable) { return 0 }
    if (-not (Set-ElmReceiveMode $Mode) -or -not (Set-ElmHeader $RequestId)) {
        $elm.LastStatus = 'Error'
        return 0
    }
    Set-ElmTiming $functional
    if ($uds) { $null = Set-ElmFlowControl $RequestId $WaitForId }

    $command = -join ($Request | ForEach-Object { '{0:X2}' -f $_ })
    $pendingWithoutAnswer = Invoke-ElmExchange $command $RequestId $Mode
    if ($pendingWithoutAnswer -and $uds -and -not $elm.LongTimeout) {
        $elm.PendingHandled = $false
        if (Invoke-ElmCommandOk 'ATSTFF') {
            $elm.LongTimeout = $true
            $elm.Collector = New-ElmCollector
            $null = Invoke-ElmExchange $command $RequestId $Mode
        }
    }
    return Get-CompleteMessageCount
}

# Prepara l'adattatore: eco e linefeed spenti, spazi e header attivi, niente DLC, protocollo automatico; legge le sigle
# (anche STI degli STN) e cerca il protocollo con 0100. Poi fissa il protocollo trovato e verifica ATSH come elmInit(),
# fermandosi se il veicolo non usa il CAN a 11 bit o se le righe non portano l'header.
function Initialize-ElmAdapter($SerialPort) {
    $elm = $script:ElmState
    $null = Invoke-ElmCommand $SerialPort 'ATZ' 3000
    foreach ($setupCommand in @('ATE0', 'ATL0', 'ATS1', 'ATH1', 'ATD0', 'ATAT0', 'ATSP0')) {
        $null = Invoke-ElmCommand $SerialPort $setupCommand
    }
    $adapterId = @(Invoke-ElmCommand $SerialPort 'ATI') -join ' '
    $deviceDescription = @(Invoke-ElmCommand $SerialPort 'AT@1') -join ' '
    $stnIdentity = @(Invoke-ElmCommand $SerialPort 'STI') -join ' '
    $elm.StnAvailable = $stnIdentity -ne '' -and $stnIdentity -notmatch '\?'
    $stnText = if ($elm.StnAvailable) { $stnIdentity } else { 'non risponde' }
    Add-ReportLine "Adattatore: $($SerialPort.PortName) @ $($SerialPort.BaudRate) | ATI: $adapterId | AT@1: $deviceDescription | STI: $stnText"

    # La prima richiesta OBD avvia la ricerca del protocollo, che può richiedere alcuni secondi
    $ecuFound = $false
    $withoutHeader = $false
    for ($attempt = 1; $attempt -le 3 -and -not $ecuFound; $attempt++) {
        Write-Host "Ricerca del protocollo e delle centraline, tentativo $attempt di 3"
        $lines = @(Invoke-ElmCommand $SerialPort '0100' 15000)
        $ecuFound = (ConvertFrom-CanLines $lines).Count -gt 0
        if (@($lines | Where-Object { $_ -match '^41 ?00' }).Count -gt 0) { $withoutHeader = $true }
    }
    $protocolNumber = (@(Invoke-ElmCommand $SerialPort 'ATDPN') -join '') -replace '^A', ''
    $protocolDescription = @(Invoke-ElmCommand $SerialPort 'ATDP') -join ' '
    $voltage = @(Invoke-ElmCommand $SerialPort 'ATRV') -join ' '
    Add-ReportLine "Protocollo: $protocolNumber $protocolDescription | Tensione: $voltage"

    if (-not $ecuFound) {
        if ($withoutHeader) { throw "L'adattatore risponde senza header CAN (ATH1): la scansione completa li richiede per distinguere le centraline." }
        if ($protocolNumber -ne '0' -and $protocolNumber -notin @('6', '8')) {
            throw "Il veicolo usa il protocollo $protocolNumber ($protocolDescription): lo script interpreta solo il CAN 11 bit (protocolli 6 e 8)."
        }
        throw 'Nessuna centralina risponde a 0100: controlla che il quadro sia acceso.'
    }
    if ($protocolNumber -notin @('6', '8')) { throw "Protocollo $protocolNumber non riconosciuto dopo la risposta a 0100." }

    # Il protocollo trovato si fissa, così una richiesta senza risposta non fa ripartire la ricerca automatica; il gemello
    # a 29 bit (7 per il 6, 9 per l'8) serve alle fasi a 29 bit, dopo le quali si torna a questo
    $elm.Protocol11 = $protocolNumber
    $elm.Protocol29 = if ($protocolNumber -eq '8') { '9' } else { '7' }
    if (-not (Invoke-ElmCommandOk ('ATSP' + $elm.Protocol11))) { throw "L'adattatore non accetta ATSP$($elm.Protocol11)." }
    $elm.CurrentHeader = $null
    $elm.PhysicalAvailable = Invoke-ElmCommandOk 'ATSH7E0'
    if ($elm.PhysicalAvailable) { $elm.CurrentHeader = '7E0' }
    else { Add-ReportLine 'ELM327: ATSH non supportato, solo richieste funzionali' }
}

# ============================================================
# Richieste della scansione: un solo punto applica le regole failsafe
# ============================================================

# Regola failsafe 5: conta gli errori di trasporto consecutivi della fase e al limite del firmware la interrompe con il
# marcatore che Invoke-ScanPhase riconosce; ogni richiesta andata a buon fine azzera il conteggio.
function Register-TransportOutcome([bool] $IsError) {
    $scan = $script:ScanState
    if (-not $IsError) {
        $scan.TransportErrors = 0
        return
    }
    $scan.TransportErrors++
    if ($scan.TransportErrors -ge $script:ScanTables.Constants['OBD_SCAN_TRANSPORT_ERROR_LIMIT']) { throw $script:PhaseSkippedMarker }
}

# Invia una richiesta e ne classifica l'esito come obdScanRequest(): risposta positiva o negativa dell'ID atteso,
# multi-frame rimasto a metà (fermo al first frame o con un frame perso), silenzio, errore di trasporto.
function Send-ScanRequest([string] $RequestId, [string] $ResponseId, [string] $Mode, [byte[]] $Request) {
    $reply = @{ Result = 'Silent'; Nrc = 0; Message = $null; Slot = $null }
    $null = Invoke-ObdTransact $RequestId $Request $ResponseId $Mode
    if ($script:ElmState.LastStatus -eq 'Error') {
        $reply.Result = 'Transport'
        Register-TransportOutcome $true
        return $reply
    }
    Register-TransportOutcome $false
    $slot = Get-CompleteMessage $ResponseId
    if ($null -ne $slot -and $slot.Data.Count -ge 1) {
        $data = $slot.Data.ToArray()
        if ($data.Length -ge 3 -and $data[0] -eq 0x7F) {
            $reply.Result = 'Negative'
            $reply.Nrc = [int]$data[2]
        }
        elseif ($data[0] -eq (($Request[0] + 0x40) -band 0xFF)) {
            $reply.Result = 'Positive'
            $reply.Message = $data
        }
        return $reply
    }
    foreach ($candidate in $script:ElmState.Collector.Slots) {
        if ($candidate.Id -eq $ResponseId -and $candidate.State -ne 'Idle') {
            $reply.Result = 'Incomplete'
            $reply.Slot = $candidate
        }
    }
    return $reply
}

# Richiesta funzionale 7DF a tutte le centraline OBD che rispondono così, contando gli errori di trasporto; ritorna
# $false senza inviare se nessuna centralina la ascolterebbe.
function Send-ScanFunctional([byte[]] $Request) {
    if (@($script:ScanState.ObdEcus | Where-Object { $_.RespondsFunctional }).Count -eq 0) { return $false }
    $null = Invoke-ObdTransact '7DF' $Request $null 'OBD11'
    Register-TransportOutcome ($script:ElmState.LastStatus -eq 'Error')
    return $true
}

# Risposta di una centralina all'ultima richiesta funzionale, solo se la centralina risponde in funzionale.
function Get-FunctionalReplyData($Ecu) {
    if (-not $Ecu.RespondsFunctional) { return $null }
    return , (Get-CompleteMessageData $Ecu.Id)
}

# Richiesta a una centralina OBD: fisica se il trasporto lo consente e la centralina risponde in fisico, altrimenti
# funzionale attendendo solo la sua risposta.
function Send-ScanObdRequest($Ecu, [byte[]] $Request) {
    $requestId = '7DF'
    if ($Ecu.RespondsPhysical -and $script:ElmState.PhysicalAvailable) { $requestId = '{0:X3}' -f ([Convert]::ToInt32($Ecu.Id, 16) - 8) }
    return Send-ScanRequest $requestId $Ecu.Id 'OBD11' $Request
}

# Vero se l'auto-test ha trovato guasto il flow control: le letture UDS su più frame si saltano (regola 6).
function Test-SingleFrameOnly {
    return $script:ScanState.FlowControlTest -eq 'Broken'
}

# Richiesta a una centralina UDS con le regole failsafe di obdScanUdsRequest(): muta, oltre il budget o con il servizio
# escluso non riceve richieste; NRC 21 si ripete una volta; 3 silenzi di fila la marcano muta; NRC 11, 7F, 7E o 12
# escludono il resto del servizio. NRC, risposta incompleta e positiva azzerano i silenzi.
function Send-ScanUdsRequest($Ecu, [byte[]] $Request) {
    $tables = $script:ScanTables
    $skipped = @{ Result = 'Silent'; Nrc = 0; Message = $null; Slot = $null }
    $serviceFlag = $null
    if ($Request[0] -eq 0x22) { $serviceFlag = 'Skip22' }
    elseif ($Request[0] -eq 0x19) { $serviceFlag = 'Skip19' }
    if ($Ecu.Mute -or $Ecu.Budget -or ($null -ne $serviceFlag -and $Ecu[$serviceFlag])) { return $skipped }
    if ((Get-ScanMilliseconds) - $Ecu.StartedMs -gt $tables.Constants['OBD_SCAN_ECU_BUDGET_MS']) {
        $Ecu.Budget = $true
        Add-ReportLine ((Get-UdsTag $Ecu) + ('budget di {0} s esaurito, letture restanti saltate' -f ($tables.Constants['OBD_SCAN_ECU_BUDGET_MS'] / 1000)))
        return $skipped
    }
    $mode = if ($Ecu.Extended) { 'UDS29' } else { 'UDS11' }
    $reply = Send-ScanRequest $Ecu.RequestId $Ecu.ResponseId $mode $Request
    # NRC 21 (centralina occupata): un solo nuovo tentativo, poi l'identificatore si salta
    if ($reply.Result -eq 'Negative' -and $reply.Nrc -eq 0x21) { $reply = Send-ScanRequest $Ecu.RequestId $Ecu.ResponseId $mode $Request }
    if ($reply.Result -eq 'Silent') {
        $Ecu.Silence++
        if ($Ecu.Silence -ge $tables.Constants['OBD_SCAN_SILENCE_LIMIT']) {
            $Ecu.Mute = $true
            Add-ReportLine ((Get-UdsTag $Ecu) + 'non risponde più: letture restanti saltate')
        }
        return $reply
    }
    if ($reply.Result -ne 'Transport') { $Ecu.Silence = 0 }
    if ($reply.Result -eq 'Negative' -and $null -ne $serviceFlag -and @(0x11, 0x7F, 0x7E, 0x12) -contains $reply.Nrc) {
        $Ecu[$serviceFlag] = $true
        Add-ReportLine ((Get-UdsTag $Ecu) + ('servizio {0:X2} non supportato (NRC {1:X2}), richieste restanti saltate' -f $Request[0], $reply.Nrc))
    }
    return $reply
}

# ============================================================
# Stampa dei valori decodificati
# ============================================================

# Vero se il segnale va mostrato, come obdbSignalVisible(): il suo bit di supporto, se c'è, vale 1 e, per i monitor
# del PID 01/41 definiti solo per l'accensione comandata, il selettore CIM vale 0; senza dati per leggerli non si mostra.
function Test-SignalVisible($Signal, $Gate, $Cim, [byte[]] $Payload) {
    if ($null -ne $Gate) {
        $raw = Get-ObdbRawBits $Payload ([int]$Gate.bix) ([int]$Gate.len) $false $false
        if ($null -eq $raw -or $raw -ne 1) { return $false }
    }
    if ($Signal.sparkOnly) {
        if ($null -eq $Cim) { return $false }
        $raw = Get-ObdbRawBits $Payload ([int]$Cim.bix) ([int]$Cim.len) $false $false
        if ($null -eq $raw -or $raw -ne 0) { return $false }
    }
    return $true
}

# Decodifica e scrive i segnali visibili di un comando OBDb dal payload (byte dopo servizio ed eco), come
# obdPrintCommandSignals(): valori con unità, booleani, enumerazioni, DTC del freeze frame, con l'etichetta delle catture
# per i DID VAG. I bit di supporto non si stampano. Ritorna quanti valori ha scritto.
function Write-CommandSignals($Command, [byte[]] $Payload, [string] $Prefix) {
    $tables = $script:ScanTables
    $printed = 0
    foreach ($signal in $Command.Signals) {
        if ($signal.kind -eq 'gate') { continue }
        $gate = $null
        if ($signal.gate -and $Command.SignalById.ContainsKey([string]$signal.gate)) { $gate = $Command.SignalById[[string]$signal.gate] }
        if (-not (Test-SignalVisible $signal $gate $Command.Cim $Payload)) { continue }
        $text = $null
        if ($signal.kind -eq 'enum') {
            $result = ConvertFrom-ObdbSignal $signal $Payload
            if ($null -ne $result) {
                $text = $result.Code
                if ($result.Text) { $text = $result.Text }
            }
        }
        elseif ($signal.kind -eq 'boolean') {
            $raw = Get-ObdbRawBits $Payload ([int]$signal.bix) ([int]$signal.len) $false $false
            if ($null -ne $raw) { $text = $tables.Booleans[[string]$signal.unit][[int]($raw -ne 0)] }
        }
        elseif ($signal.kind -eq 'dtc') {
            $raw = Get-ObdbRawBits $Payload ([int]$signal.bix) ([int]$signal.len) $false $false
            if ($null -ne $raw) {
                $text = 'nessuno'
                if ($raw -ne 0) { $text = Format-DtcCode ([int]$raw) }
            }
        }
        elseif ($null -ne (Get-ObdbRawBits $Payload ([int]$signal.bix) ([int]$signal.len) ([bool]$signal.blsb) $false)) {
            $result = ConvertFrom-ObdbSignal $signal $Payload
            if ($null -eq $result) {
                $text = 'nessun valore'
            }
            else {
                $text = Format-ScanNumber $result.Value
                $symbol = $tables.Units[[string]$signal.unit]
                if ($symbol) { $text += ' ' + $symbol }
                # Il PID SAE 63 è la coppia di riferimento del motore, che il profilo suggerito confronta con quella di targa
                if ($Command.IsSae -and $Command.IdValue -eq 0x63) { $script:ScanState.ReferenceTorqueNm = $result.Value }
            }
        }
        if ($null -eq $text) { continue }
        if (-not $Command.IsSae) {
            $captureLabel = if ($Command.Captured) { 'risposto nelle catture' } else { 'assente nelle catture' }
            $text += '  ({0} OBDb A5 {1})' -f $captureLabel, $tables.ModelYear
        }
        Add-ReportLine ($Prefix + [string]$signal.name + ': ' + $text)
        $printed++
    }
    return $printed
}

# Scrive i segnali di un PID SAE di una centralina OBD come obdPrintSaePid(), con le esclusioni di scala dichiarate in
# 4F (lambda, sonde, MAP) o 50 (MAF).
function Write-SaePid($Ecu, $Command, [int] $ParameterId, [byte[]] $Payload, [bool] $Declared, [string] $Label) {
    $undeclared = if ($Declared) { '' } else { ' (non dichiarato)' }
    $prefix = (Get-ObdTag $Ecu) + ('{0} {1:X2}{2} ' -f $Label, $ParameterId, $undeclared)
    if (($Command.Depends4F -and $Ecu.ScaleFrom4F) -or ($Command.Depends50 -and $Ecu.ScaleFrom50)) {
        Add-ReportLine ($prefix + 'scala non predefinita dichiarata in 4F/50: valore non mostrato')
        return
    }
    $null = Write-CommandSignals $Command $Payload $prefix
}

# Scrive i test di un monitor Mode 06 come obdPrintMode06Tests(): esito di tutti, valore e limiti solo per i TID
# standard 01-0C con UASID noto.
function Write-Mode06Tests([byte[]] $Message, [string] $Prefix) {
    $tests = Get-Mode06Tests $Message
    if ($tests.Count -eq 0) { return }
    $name = Get-Mode06MonitorName $tests[0].Mid
    if ($null -eq $name) { return }
    $passed = 0
    $failed = 0
    $unknown = 0
    foreach ($test in $tests) {
        $outcome = Get-Mode06Outcome $test
        if ($outcome -eq 1) { $passed++ } elseif ($outcome -eq 0) { $failed++ } else { $unknown++ }
    }
    $line = $Prefix + 'Monitor ' + $name + ": $passed test superati, $failed falliti"
    if ($unknown -gt 0) { $line += ", $unknown con scala non definita" }
    Add-ReportLine $line
    foreach ($test in $tests) {
        $uas = Get-UasidEntry $test.Uasid
        if ($test.Tid -lt 0x01 -or $test.Tid -gt 0x0C -or $null -eq $uas) { continue }
        Add-ReportLine ($Prefix + '    ' + $script:ScanTables.TidNames[$test.Tid - 1] + ': ' + (Format-ScanNumber (Get-UasidValue $uas $test.Value)) +
            ' (limiti ' + (Format-ScanNumber (Get-UasidValue $uas $test.Min)) + ' - ' + (Format-ScanNumber (Get-UasidValue $uas $test.Max)) + ') ' + $uas.Unit)
    }
}

# Scrive una InfoType Mode 09 interpretabile come obdPrintInfoType(): ID calibrazione come testo, contatori IPT con il
# nome SAE, OBFCM solo se plausibile. Data è la risposta dopo 49 e InfoType.
function Write-InfoType([int] $InfoType, [byte[]] $Data, [string] $Prefix) {
    $tables = $script:ScanTables
    if ($InfoType -eq 0x04 -and $Data.Length -ge 1) {
        for ($position = 1; $position + 16 -le $Data.Length; $position += 16) {
            $text = ConvertFrom-PrintableText (Get-ByteSlice $Data $position 16) 23
            if ($null -ne $text) { Add-ReportLine ($Prefix + 'ID calibrazione software: ' + $text) }
        }
    }
    elseif ($InfoType -eq 0x08 -or $InfoType -eq 0x0B) {
        $names = $tables.IptCompression
        if ($InfoType -eq 0x08) { $names = $tables.IptSpark }
        $line = $Prefix + 'Contatori di completamento dei monitor (IPT):'
        $nameIndex = 0
        for ($position = 1; $position + 2 -le $Data.Length -and $nameIndex -lt $names.Count; $position += 2) {
            # Il buffer del firmware tiene 11 caratteri per nome
            $name = $names[$nameIndex]
            if ($name.Length -gt 11) { $name = $name.Substring(0, 11) }
            if ($name.Length -eq 0) { break }
            $line += ' {0}={1}' -f $name, (([int]$Data[$position] -shl 8) -bor $Data[$position + 1])
            $nameIndex++
        }
        Add-ReportLine $line
    }
    elseif ($InfoType -eq 0x17) {
        $copy = [math]::Min($Data.Length, $tables.Constants['OBD_MAX_MESSAGE_LEN'] - 2)
        [byte[]]$message = @(0x49, 0x17) + (Get-ByteSlice $Data 0 $copy)
        $counters = ConvertFrom-Obfcm $message
        if ($null -eq $counters -or -not $counters.Plausible) {
            Add-ReportLine ($Prefix + 'Consumo reale OBFCM: layout non plausibile, non mostrato')
            return
        }
        $script:ScanState.ObfcmAvailable = $true
        Add-ReportLine ($Prefix + 'Consumo reale di vita (OBFCM): ' + (Format-ScanNumber $counters.LifetimeDistanceKm) + ' km, ' +
            (Format-ScanNumber $counters.LifetimeFuelL) + ' L, media ' + (Format-ScanNumber $counters.AverageLitersPer100) + ' L/100 km')
    }
}

# Memorizza e scrive i DTC di tipo Kind (0 confermati, 1 in attesa, 2 permanenti) di una centralina OBD come
# obdStoreObdDtcs(): positiva con codici e descrizioni, negativa come servizio non supportato; senza risposta restano non letti.
function Write-ObdDtcs($Ecu, [int] $Kind, $Message) {
    if ($null -eq $Message -or $Message.Length -lt 1) { return }
    $services = @(0x03, 0x07, 0x0A)
    $positive = $Message[0] -eq ($services[$Kind] + 0x40)
    if (-not $positive -and $Message[0] -ne 0x7F) { return }
    $label = @('DTC confermati: ', 'DTC in attesa: ', 'DTC permanenti: ')[$Kind]
    if (-not $positive) {
        Add-ReportLine ((Get-ObdTag $Ecu) + $label + 'servizio non supportato')
        return
    }
    $codes = Get-ObdDtcCodes ([byte[]]$Message)
    $Ecu.DtcRead[$Kind] = $true
    $Ecu.DtcCodes[$Kind] = $codes
    $descriptions = New-Object System.Collections.Generic.List[string]
    foreach ($code in $codes) {
        $text = Format-DtcCode $code
        if ($script:ScanTables.DtcNames.ContainsKey($text)) { $text += ' (' + $script:ScanTables.DtcNames[$text] + ')' }
        $descriptions.Add($text)
    }
    $list = if ($descriptions.Count -gt 0) { $descriptions -join ', ' } else { 'nessuno' }
    Add-ReportLine ((Get-ObdTag $Ecu) + $label + $list)
}

# Scrive un DID di identificazione UDS come obdPrintIdentification(): date BCD (o come testo), sessione attiva per
# nome, gli altri come testo; un contenuto binario non si mostra. Il nome di sistema F197 diventa anche il nome della centralina.
function Write-Identification($Ecu, [int] $Did, [string] $Name, [byte[]] $Data) {
    $text = $null
    if ($Did -eq 0xF186) {
        if ($Data.Length -eq 1) {
            if ($Data[0] -eq 0x01) { $text = 'predefinita' }
            elseif ($Data[0] -eq 0x02) { $text = 'programmazione' }
            elseif ($Data[0] -eq 0x03) { $text = 'estesa' }
        }
    }
    elseif (@(0xF18B, 0xF199, 0xF19B, 0xF19D) -contains $Did) {
        $text = ConvertFrom-BcdDate $Data
        if ($null -eq $text) { $text = ConvertFrom-PrintableText $Data 47 }
    }
    else {
        $text = ConvertFrom-PrintableText $Data 47
    }
    if ($null -eq $text) { return }
    if ($Did -eq 0xF197) {
        $Ecu.SystemName = $text
        if ($text.Length -gt 20) { $Ecu.SystemName = $text.Substring(0, 20) }
    }
    Add-ReportLine ((Get-UdsTag $Ecu) + $Name + ': ' + $text)
}

# Scrive la nota di una risposta UDS rimasta incompleta, che non si decodifica mai.
function Write-IncompleteReply($Ecu, [int] $Did) {
    Add-ReportLine ((Get-UdsTag $Ecu) + ("22 {0:X4}: risposta non completata dall'adattatore" -f $Did))
}

# ============================================================
# Fasi della scansione completa, nell'ordine del firmware
# ============================================================

# Esegue una fase con il proprio conteggio degli errori di trasporto: al quinto di fila il firmware passa alla fase
# successiva con una riga nel report, e qui accade lo stesso.
function Invoke-ScanPhase([string] $Name, [scriptblock] $Body) {
    $script:ScanState.TransportErrors = 0
    Write-Host "-- Fase: $Name" -ForegroundColor Cyan
    try {
        $null = & $Body
    }
    catch {
        if ($_.Exception.Message -ne $script:PhaseSkippedMarker) { throw }
        Add-ReportLine "  Errori di trasporto ripetuti, fase saltata: $Name"
    }
}

# Cerca una centralina OBD per ID di risposta; $null se non trovata.
function Find-ObdEcu([string] $EcuId) {
    foreach ($ecu in $script:ScanState.ObdEcus) {
        if ($ecu.Id -eq $EcuId) { return $ecu }
    }
    return $null
}

# Restituisce la centralina OBD dell'ID indicato, aggiungendola se nuova; $null se l'elenco del firmware è pieno.
function Add-ObdEcu([string] $EcuId) {
    $ecu = Find-ObdEcu $EcuId
    if ($null -ne $ecu -or $script:ScanState.ObdEcus.Count -ge $script:ScanTables.Constants['OBD_MAX_ECUS']) { return $ecu }
    $ecu = @{
        Id                 = $EcuId
        Mode01             = New-Object byte[] 32
        Mode01Verified     = New-Object byte[] 32
        Mode02             = New-Object byte[] 32
        Mode06             = New-Object byte[] 32
        Mode09             = New-Object byte[] 32
        Name               = ''
        RespondsFunctional = $false
        RespondsPhysical   = $false
        ScaleFrom4F        = $false
        ScaleFrom50        = $false
        DtcRead            = New-Object bool[] 3
        DtcCodes           = @($null, $null, $null)
    }
    $script:ScanState.ObdEcus.Add($ecu)
    return $ecu
}

# Bitmap dichiarata del servizio OBD indicato (01, 02, 06 o 09).
function Get-ServiceBitmap($Ecu, [int] $Service) {
    switch ($Service) {
        0x02 { return , $Ecu.Mode02 }
        0x06 { return , $Ecu.Mode06 }
        0x09 { return , $Ecu.Mode09 }
    }
    return , $Ecu.Mode01
}

# Fasi 1-2: centraline OBD con due 0100 funzionali e con 0100 fisico a 7E0-7E7, che il firmware invia senza contare
# gli errori di trasporto.
function Invoke-PhaseObdDiscovery {
    $scan = $script:ScanState
    $request = [byte[]]@(0x01, 0x00)
    for ($attempt = 0; $attempt -lt 2; $attempt++) {
        $null = Invoke-ObdTransact '7DF' $request $null 'OBD11'
        foreach ($slot in $script:ElmState.Collector.Slots) {
            if ($slot.State -ne 'Complete') { continue }
            $four = Get-SupportedRange $slot.Data.ToArray() 0x41 0x00
            if ($null -eq $four) { continue }
            $ecu = Add-ObdEcu $slot.Id
            if ($null -eq $ecu) { continue }
            $ecu.RespondsFunctional = $true
            Set-BitmapRange $ecu.Mode01 0x00 $four
        }
    }
    for ($offset = 0; $offset -lt 8 -and $script:ElmState.PhysicalAvailable; $offset++) {
        $ecuId = '{0:X3}' -f (0x7E8 + $offset)
        $null = Invoke-ObdTransact ('{0:X3}' -f (0x7E0 + $offset)) $request $ecuId 'OBD11'
        $data = Get-CompleteMessageData $ecuId
        if ($null -eq $data) { continue }
        $four = Get-SupportedRange $data 0x41 0x00
        if ($null -eq $four) { continue }
        $ecu = Add-ObdEcu $ecuId
        if ($null -eq $ecu) { continue }
        $ecu.RespondsPhysical = $true
        Set-BitmapRange $ecu.Mode01 0x00 $four
    }
    Add-ReportLine "Centraline OBD: $($scan.ObdEcus.Count)"
    foreach ($ecu in $scan.ObdEcus) {
        Add-ReportLine ((Get-ObdTag $ecu) + 'risponde in funzionale ' + (Format-YesNo $ecu.RespondsFunctional) + ', in fisico ' + (Format-YesNo $ecu.RespondsPhysical))
    }
}

# Fase 3: catene delle bitmap dei servizi 01, 02 (freeze frame 0), 06 e 09, un range per richiesta: funzionale per
# tutte, fisica alle centraline che rispondono solo così. Una catena si ferma quando nessuna dichiara il range successivo.
function Invoke-PhaseBitmaps {
    $ecus = $script:ScanState.ObdEcus
    foreach ($service in @(0x01, 0x02, 0x06, 0x09)) {
        for ($base = 0x00; $base -le 0xE0; $base += 0x20) {
            $expected = $base -eq 0
            foreach ($ecu in $ecus) {
                if (Test-BitmapId (Get-ServiceBitmap $ecu $service) $base) { $expected = $true }
            }
            if (-not $expected) { break }
            [byte[]]$request = @($service, $base)
            if ($service -eq 0x02) { [byte[]]$request = @($service, $base, 0x00) }
            $null = Send-ScanFunctional $request
            foreach ($ecu in $ecus) {
                $data = Get-FunctionalReplyData $ecu
                if ($null -eq $data) { continue }
                $four = Get-SupportedRange $data ($service + 0x40) $base
                if ($null -ne $four) { Set-BitmapRange (Get-ServiceBitmap $ecu $service) $base $four }
            }
            foreach ($ecu in $ecus) {
                if ($ecu.RespondsFunctional -or -not $ecu.RespondsPhysical) { continue }
                $reply = Send-ScanObdRequest $ecu $request
                if ($reply.Result -ne 'Positive') { continue }
                $four = Get-SupportedRange $reply.Message ($service + 0x40) $base
                if ($null -ne $four) { Set-BitmapRange (Get-ServiceBitmap $ecu $service) $base $four }
            }
        }
    }
}

# Fase 4: legge 4F e 50 dove dichiarati. Valori diversi da zero sono massimi non predefiniti, che cambiano la scala di
# lambda, sonde e MAP (4F) o del MAF (50): quei PID non si mostrano.
function Invoke-PhaseScale {
    foreach ($ecu in $script:ScanState.ObdEcus) {
        foreach ($parameterId in @(0x4F, 0x50)) {
            if (-not (Test-BitmapId $ecu.Mode01 $parameterId)) { continue }
            $reply = Send-ScanObdRequest $ecu ([byte[]]@(0x01, $parameterId))
            if ($reply.Result -ne 'Positive' -or $reply.Message.Length -lt 6) { continue }
            $nonDefault = $false
            for ($index = 2; $index -lt $reply.Message.Length; $index++) {
                if ($reply.Message[$index] -ne 0) { $nonDefault = $true }
            }
            if ($parameterId -eq 0x4F) { $ecu.ScaleFrom4F = $nonDefault } else { $ecu.ScaleFrom50 = $nonDefault }
        }
    }
}

# Fase 5: ogni PID SAE della selezione, dichiarato o no, in funzionale per tutte le centraline insieme; quelle
# raggiungibili solo in fisico ricevono i PID che dichiarano. Un PID non dichiarato che tace non marca nessuno.
function Invoke-PhaseMode01 {
    $ecus = $script:ScanState.ObdEcus
    foreach ($command in $script:ScanTables.SaeCommands) {
        $parameterId = $command.IdValue
        [byte[]]$request = @(0x01, $parameterId)
        $null = Send-ScanFunctional $request
        foreach ($ecu in $ecus) {
            $data = Get-FunctionalReplyData $ecu
            if ($null -eq $data -or $data.Length -lt 2 -or $data[0] -ne 0x41 -or $data[1] -ne $parameterId) { continue }
            Set-BitmapId $ecu.Mode01Verified $parameterId
            Write-SaePid $ecu $command $parameterId (Get-ByteSlice $data 2) (Test-BitmapId $ecu.Mode01 $parameterId) 'PID'
        }
        foreach ($ecu in $ecus) {
            if ($ecu.RespondsFunctional -or -not $ecu.RespondsPhysical -or -not (Test-BitmapId $ecu.Mode01 $parameterId)) { continue }
            $reply = Send-ScanObdRequest $ecu $request
            if ($reply.Result -ne 'Positive' -or $reply.Message.Length -lt 2 -or $reply.Message[1] -ne $parameterId) { continue }
            Set-BitmapId $ecu.Mode01Verified $parameterId
            Write-SaePid $ecu $command $parameterId (Get-ByteSlice $reply.Message 2) $true 'PID'
        }
    }
}

# Fase 5bis: prova sul motore di una richiesta con 2 e una con 6 PID dichiarati fra i candidati del profilo; le risposte
# complete e ben formate decidono OBD_ENGINE_MULTI_PID. Con meno di 6 PID dichiarati vale l'esito della prova a 2.
function Invoke-PhaseMultiPid {
    $scan = $script:ScanState
    $engine = Find-ObdEcu '7E8'
    if ($null -eq $engine) { return }
    foreach ($wanted in @(2, 6)) {
        $selected = New-Object System.Collections.Generic.List[int]
        foreach ($candidate in $script:ScanTables.ProfileCandidates) {
            if ($selected.Count -lt $wanted -and (Test-BitmapId $engine.Mode01 $candidate) -and $script:ScanTables.PidDataLength[$candidate] -ne 0) { $selected.Add($candidate) }
        }
        if ($selected.Count -eq $wanted) {
            [byte[]]$request = @(0x01) + $selected.ToArray()
            $reply = Send-ScanObdRequest $engine $request
            $ok = $false
            if ($reply.Result -eq 'Positive') {
                $check = Get-MultiPidCheck $reply.Message
                $ok = $check.WellFormed -and $check.FieldCount -eq $wanted
            }
            if ($wanted -eq 2) { $scan.MultiPid2 = $ok } else { $scan.MultiPid6 = $ok }
            $outcome = if ($ok) { 'risposta completa' } else { 'risposta assente o incompleta' }
            Add-ReportLine ((Get-ObdTag $engine) + "richiesta con $wanted PID: $outcome")
        }
        elseif ($wanted -eq 6) {
            $scan.MultiPid6 = $scan.MultiPid2
        }
    }
}

# Fase 6: freeze frame per centralina. Prima 02 02 00, il DTC che l'ha generato: con 0000 non c'è freeze frame; poi i
# PID dichiarati in Mode 02 e selezionati, con la risposta [42 PID 00 dati].
function Invoke-PhaseMode02 {
    foreach ($ecu in $script:ScanState.ObdEcus) {
        $declared = $false
        for ($id = 1; $id -le 0xFF -and -not $declared; $id++) { $declared = (-not (Test-RangeId $id)) -and (Test-BitmapId $ecu.Mode02 $id) }
        if (-not $declared) { continue }
        $reply = Send-ScanObdRequest $ecu ([byte[]]@(0x02, 0x02, 0x00))
        $frozen = $reply.Result -eq 'Positive' -and $reply.Message.Length -ge 5 -and (($reply.Message[3] -bor $reply.Message[4]) -ne 0)
        if (-not $frozen) { continue }
        foreach ($command in $script:ScanTables.SaeCommands) {
            $parameterId = $command.IdValue
            if (-not (Test-BitmapId $ecu.Mode02 $parameterId)) { continue }
            $reply = Send-ScanObdRequest $ecu ([byte[]]@(0x02, $parameterId, 0x00))
            if ($reply.Result -eq 'Positive' -and $reply.Message.Length -ge 3 -and $reply.Message[1] -eq $parameterId) {
                Write-SaePid $ecu $command $parameterId (Get-ByteSlice $reply.Message 3) $true 'Freeze frame PID'
            }
        }
    }
}

# Fase 7: monitor Mode 06 dichiarati che lo standard nomina, uno per richiesta per ogni centralina.
function Invoke-PhaseMode06 {
    foreach ($ecu in $script:ScanState.ObdEcus) {
        for ($mid = 1; $mid -le 0xFF; $mid++) {
            if ((Test-RangeId $mid) -or -not (Test-BitmapId $ecu.Mode06 $mid) -or $null -eq (Get-Mode06MonitorName $mid)) { continue }
            $reply = Send-ScanObdRequest $ecu ([byte[]]@(0x06, $mid))
            if ($reply.Result -eq 'Positive') { Write-Mode06Tests $reply.Message (Get-ObdTag $ecu) }
        }
    }
}

# Fase 8: InfoType Mode 09 interpretabili (04, 08, 0A, 0B, 17) dichiarate da ogni centralina; mai la 02 del VIN.
function Invoke-PhaseMode09 {
    foreach ($ecu in $script:ScanState.ObdEcus) {
        foreach ($infoType in @(0x04, 0x08, 0x0A, 0x0B, 0x17)) {
            if (-not (Test-BitmapId $ecu.Mode09 $infoType)) { continue }
            $reply = Send-ScanObdRequest $ecu ([byte[]]@(0x09, $infoType))
            if ($reply.Result -ne 'Positive' -or $reply.Message.Length -lt 3 -or $reply.Message[1] -ne $infoType) { continue }
            $prefix = Get-ObdTag $ecu
            if ($infoType -eq 0x0A) {
                $name = Get-EcuNameText $reply.Message
                if ($null -ne $name) {
                    $ecu.Name = $name
                    Add-ReportLine ($prefix + 'Nome centralina: ' + $name)
                }
                continue
            }
            Write-InfoType $infoType (Get-ByteSlice $reply.Message 2) $prefix
        }
    }
}

# Fase 9: DTC confermati (03), in attesa (07) e permanenti (0A), in funzionale per chi risponde così e in fisico per le
# altre centraline.
function Invoke-PhaseObdDtc {
    $ecus = $script:ScanState.ObdEcus
    $services = @(0x03, 0x07, 0x0A)
    for ($kind = 0; $kind -lt 3; $kind++) {
        [byte[]]$request = @($services[$kind])
        $null = Send-ScanFunctional $request
        foreach ($ecu in $ecus) { Write-ObdDtcs $ecu $kind (Get-FunctionalReplyData $ecu) }
        foreach ($ecu in $ecus) {
            if ($ecu.RespondsFunctional -or -not $ecu.RespondsPhysical) { continue }
            $reply = Send-ScanObdRequest $ecu $request
            $message = $null
            if ($reply.Result -eq 'Positive' -or $reply.Result -eq 'Negative') { $message = Get-CompleteMessageData $ecu.Id }
            Write-ObdDtcs $ecu $kind $message
        }
    }
}

# Fase 10: 0100 sull'indirizzo funzionale a 29 bit, solo per sapere se qualche centralina risponde così.
function Invoke-PhaseObd29 {
    $scan = $script:ScanState
    $elm = $script:ElmState
    if ($elm.ExtendedAvailable) { $scan.Obd29Responders = Invoke-ObdTransact '18DB33F1' ([byte[]]@(0x01, 0x00)) $null 'OBD29' }
    # L'adattatore può rifiutare il 29 bit proprio in questa richiesta: flag ed esito si guardano dopo
    if (-not $elm.ExtendedAvailable -or $elm.LastStatus -eq 'Error') { Add-ReportLine "OBD a 29 bit: non supportato dall'adattatore, saltato" }
    else { Add-ReportLine "OBD a 29 bit: $($scan.Obd29Responders) centraline rispondono" }
}

# Restituisce la centralina UDS con quella richiesta, aggiungendola se nuova, come obdUdsFindOrAdd(); $null se l'elenco
# del firmware è pieno. La coppia 7E0+n/7E8+n si lega alla centralina OBD corrispondente, che compare una volta sola.
function Add-UdsEcu([string] $RequestId, [string] $ResponseId) {
    $scan = $script:ScanState
    foreach ($existing in $scan.UdsEcus) {
        if ($existing.RequestId -eq $RequestId) { return $existing }
    }
    if ($scan.UdsEcus.Count -ge $script:ScanTables.Constants['OBD_MAX_UDS_ECUS']) { return $null }
    $extended = $RequestId.Length -eq 8
    $odis = $null
    if (-not $extended) { $odis = $script:ScanTables.OdisByRequest[$RequestId] }
    $obdEcu = $null
    if (Test-ObdResponseId $ResponseId) { $obdEcu = Find-ObdEcu $ResponseId }
    $ecu = @{
        RequestId    = $RequestId
        ResponseId   = $ResponseId
        Extended     = $extended
        Odis         = $odis
        ObdEcu       = $obdEcu
        Number       = $scan.UdsEcus.Count + 1
        SystemName   = ''
        StartedMs    = 0
        Mute         = $false
        Budget       = $false
        Skip22       = $false
        Skip19       = $false
        SkipF4       = $false
        Failure      = $null
        Silence      = 0
        F4NrcStreak  = 0
        F4LastNrc    = 0
        StdPids      = 0
        VagDids      = 0
        DtcConfirmed = $null
        DtcPending   = $null
        DtcFailed    = $null
    }
    $scan.UdsEcus.Add($ecu)
    return $ecu
}

# Prima slot valida della discovery, come obdProbeStepUdsDiscovery(): un first frame o una risposta completa 7E 00,
# 7F 3E, 7F 22 o 62 F1 97, solo dall'ID ODIS atteso quando c'è. Ritorna l'ID di risposta o $null.
function Find-DiscoveryResponder([string] $ExpectedId) {
    foreach ($slot in $script:ElmState.Collector.Slots) {
        if ($ExpectedId -and $slot.Id -ne $ExpectedId) { continue }
        $data = $slot.Data
        $complete = $slot.State -eq 'Complete' -and $data.Count -ge 2
        $valid = $slot.State -eq 'Receiving' -or ($complete -and (
                ($data[0] -eq 0x7E -and $data[1] -eq 0x00) -or
                ($data[0] -eq 0x7F -and ($data[1] -eq 0x3E -or $data[1] -eq 0x22)) -or
                ($data[0] -eq 0x62 -and $data.Count -ge 3 -and $data[1] -eq 0xF1 -and $data[2] -eq 0x97)))
        if ($valid) { return $slot.Id }
    }
    return $null
}

# Fasi 11-12: discovery UDS in sequenza, perchè l'ELM327 invia una richiesta per comando: 3E 00 alle richieste VAG
# 700-773 e 7E0-7E7 con attesa sull'ID ODIS atteso, oppure a 17FC0000-17FC00FF a 29 bit; una volta 22 F197 agli ID ODIS
# muti. Le risposte fuori elenco ODIS si confermano dopo. NO DATA è la norma e non ferma la ricerca.
function Invoke-PhaseUdsDiscovery([bool] $Extended) {
    $scan = $script:ScanState
    $elm = $script:ElmState
    $mode = if ($Extended) { 'UDS29' } else { 'UDS11' }
    $candidates = New-Object System.Collections.Generic.List[string]
    if ($Extended) {
        for ($low = 0x00; $low -le 0xFF; $low++) { $candidates.Add('17FC00{0:X2}' -f $low) }
    }
    else {
        for ($value = 0x700; $value -le 0x773; $value++) { $candidates.Add('{0:X3}' -f $value) }
        for ($value = 0x7E0; $value -le 0x7E7; $value++) { $candidates.Add('{0:X3}' -f $value) }
    }
    foreach ($requestId in $candidates) {
        if ($Extended -and -not $elm.ExtendedAvailable) {
            Add-ReportLine "UDS a 29 bit: non supportato dall'adattatore, saltato"
            return
        }
        $odis = $null
        if (-not $Extended) { $odis = $script:ScanTables.OdisByRequest[$requestId] }
        $expected = $null
        if ($null -ne $odis) { $expected = $odis.ResponseId }
        $fallback = $false
        while ($true) {
            [byte[]]$request = @(0x3E, 0x00)
            if ($fallback) { [byte[]]$request = @(0x22, 0xF1, 0x97) }
            $null = Invoke-ObdTransact $requestId $request $expected $mode
            Register-TransportOutcome ($elm.LastStatus -eq 'Error')
            $responseId = Find-DiscoveryResponder $expected
            if ($null -ne $responseId) {
                if ($null -ne $odis) { $null = Add-UdsEcu $requestId $responseId }
                elseif ($scan.ConfirmList.Count -lt $script:ScanTables.Constants['OBD_MAX_UDS_ECUS']) { $scan.ConfirmList.Add($responseId) }
            }
            if ($null -eq $responseId -and $null -ne $odis -and -not $fallback) {
                $fallback = $true
                continue
            }
            break
        }
    }
}

# Richiesta attesa da una risposta fuori elenco ODIS come obdUdsInferRequest(): 17FC00xx a 29 bit, risposta - 8 per
# 7E8-7EF, risposta - 6A per le altre a 11 bit.
function Get-InferredRequestId([string] $ResponseId) {
    if ($ResponseId.Length -eq 8) { return '17FC00' + $ResponseId.Substring(6) }
    $value = [Convert]::ToInt32($ResponseId, 16)
    if (Test-ObdResponseId $ResponseId) { return '{0:X3}' -f ($value - 8) }
    return '{0:X3}' -f ($value - 0x6A)
}

# Fase 13: conferma delle coppie fuori elenco ODIS con un secondo 3E 00 alla richiesta ricavata; vale qualunque
# risposta, anche negativa o incompleta.
function Invoke-PhaseUdsConfirm {
    $scan = $script:ScanState
    foreach ($responseId in $scan.ConfirmList) {
        $requestId = Get-InferredRequestId $responseId
        $mode = if ($responseId.Length -eq 8) { 'UDS29' } else { 'UDS11' }
        $reply = Send-ScanRequest $requestId $responseId $mode ([byte[]]@(0x3E, 0x00))
        if ($reply.Result -eq 'Positive' -or $reply.Result -eq 'Negative' -or $reply.Result -eq 'Incomplete') { $null = Add-UdsEcu $requestId $responseId }
    }
    Add-ReportLine "Centraline UDS trovate: $($scan.UdsEcus.Count)"
}

# Fase 14: auto-test del flow control sul gateway 710/77A, sempre presente e diverso da 7E0/7E8 che l'ELM gestisce già:
# F187 sta su più frame. Completo vuol dire funzionante, fermo al first frame guasto, altro non conclusivo.
function Invoke-PhaseFlowControlTest {
    $scan = $script:ScanState
    $scan.FlowControlTest = 'Inconclusive'
    $scan.RegisterStnPairs = $true
    foreach ($ecu in $scan.UdsEcus) {
        if ($ecu.RequestId -ne '710') { continue }
        $reply = Send-ScanRequest '710' $ecu.ResponseId 'UDS11' ([byte[]]@(0x22, 0xF1, 0x87))
        if ($reply.Result -eq 'Positive' -and $reply.Message.Length -gt 7) { $scan.FlowControlTest = 'Ok' }
        # Guasto solo se la risposta resta al first frame: un frame perso a metà non dice nulla sul flow control
        if ($reply.Result -eq 'Incomplete' -and $reply.Slot.State -eq 'Receiving' -and $reply.Slot.NextSequence -eq 1) { $scan.FlowControlTest = 'Broken' }
    }
    $outcome = 'non verificabile, si valuta richiesta per richiesta'
    if ($scan.FlowControlTest -eq 'Ok') { $outcome = 'funziona' }
    elseif ($scan.FlowControlTest -eq 'Broken') { $outcome = 'NON funziona, le risposte lunghe resteranno incomplete' }
    Add-ReportLine "Flow control verso le centraline UDS: $outcome"
}

# Identificazione UDS: i DID con significato pubblico di obd_scan.h, come testo o data; con il flow control guasto
# resta solo la sessione attiva F186, perchè gli altri sono testi su più frame.
function Read-UdsIdentification($Ecu) {
    $tables = $script:ScanTables
    for ($index = 0; $index -lt $tables.IdentDids.Count; $index++) {
        $did = $tables.IdentDids[$index]
        if ((Test-SingleFrameOnly) -and $did -ne 0xF186) { continue }
        $reply = Send-ScanUdsRequest $Ecu ([byte[]]@(0x22, ($did -shr 8), ($did -band 0xFF)))
        if ($reply.Result -eq 'Positive' -and $reply.Message.Length -gt 3) { Write-Identification $Ecu $did $tables.IdentNames[$index] (Get-ByteSlice $reply.Message 3) }
        elseif ($reply.Result -eq 'Incomplete') { Write-IncompleteReply $Ecu $did }
    }
}

# PID standard via UDS (DID F4xx) come obdProbeUdsF4(): con F400 positivo la catena delle bitmap e i PID dichiarati e
# selezionati, altrimenti il piccolo insieme di prova; saltata sulle centraline OBD, che li hanno dati in Mode 01.
# 10 NRC uguali di fila chiudono la lettura.
function Read-UdsStandardPids($Ecu) {
    $tables = $script:ScanTables
    if ($null -ne $Ecu.ObdEcu -or $Ecu.SkipF4 -or $Ecu.Skip22) { return }
    $declared = New-Object byte[] 32
    $chainOk = $false
    for ($base = 0x00; $base -le 0xE0; $base += 0x20) {
        $reply = Send-ScanUdsRequest $Ecu ([byte[]]@(0x22, 0xF4, $base))
        if ($reply.Result -ne 'Positive' -or $reply.Message.Length -lt 7) { break }
        $chainOk = $true
        Set-BitmapRange $declared $base (Get-ByteSlice $reply.Message 3 4)
        if ($base -ge 0xE0 -or ($reply.Message[6] -band 0x01) -eq 0) { break }
    }
    $candidates = New-Object System.Collections.Generic.List[int]
    if ($chainOk) {
        for ($id = 1; $id -le 0xFF; $id++) {
            if (-not (Test-RangeId $id) -and (Test-BitmapId $declared $id) -and $tables.SaeByPid.ContainsKey($id)) { $candidates.Add($id) }
        }
    }
    else {
        foreach ($id in $tables.F4ProbeSet) {
            if ($tables.SaeByPid.ContainsKey($id)) { $candidates.Add($id) }
        }
    }
    foreach ($parameterId in $candidates) {
        if ($Ecu.SkipF4 -or $Ecu.Skip22) { return }
        $dataLength = $tables.PidDataLength[$parameterId]
        if ((Test-SingleFrameOnly) -and ($dataLength -eq 0 -or $dataLength -gt 4)) { continue }
        $reply = Send-ScanUdsRequest $Ecu ([byte[]]@(0x22, 0xF4, $parameterId))
        if ($reply.Result -eq 'Positive' -and $reply.Message.Length -ge 3) {
            $Ecu.F4NrcStreak = 0
            $Ecu.StdPids++
            $prefix = '  [{0}] PID standard F4{1:X2} ' -f (Get-UdsDisplayName $Ecu), $parameterId
            $null = Write-CommandSignals $tables.SaeByPid[$parameterId] (Get-ByteSlice $reply.Message 3) $prefix
        }
        elseif ($reply.Result -eq 'Negative') {
            if ($reply.Nrc -eq $Ecu.F4LastNrc) { $Ecu.F4NrcStreak++ } else { $Ecu.F4NrcStreak = 1 }
            $Ecu.F4LastNrc = $reply.Nrc
            if ($Ecu.F4NrcStreak -ge $tables.Constants['OBD_SCAN_F4_NRC_LIMIT']) { $Ecu.SkipF4 = $true }
        }
        elseif ($reply.Result -eq 'Incomplete') {
            Write-IncompleteReply $Ecu (0xF400 -bor $parameterId)
        }
    }
}

# 22 F600 e 22 F800, solo per segnalare se la centralina dichiara monitor o InfoType OBD via UDS.
function Read-UdsMonitorDeclarations($Ecu) {
    $reply = Send-ScanUdsRequest $Ecu ([byte[]]@(0x22, 0xF6, 0x00))
    if ($reply.Result -eq 'Positive') { Add-ReportLine ((Get-UdsTag $Ecu) + 'dichiara monitor OBD via UDS (F600)') }
    $reply = Send-ScanUdsRequest $Ecu ([byte[]]@(0x22, 0xF8, 0x00))
    if ($reply.Result -eq 'Positive') { Add-ReportLine ((Get-UdsTag $Ecu) + 'dichiara InfoType OBD via UDS (F800)') }
}

# DID VAG della selezione OBDb con la stessa coppia di ID, come obdProbeUdsObdb(); i DID F4xx si saltano sulle
# centraline OBD, che li hanno già letti in Mode 01, e con il flow control guasto quelli che non stanno in un frame.
function Read-UdsObdbDids($Ecu) {
    foreach ($command in $script:ScanTables.VagCommands) {
        if ($Ecu.Skip22 -or $Ecu.Extended) { return }
        if ($command.Header -ne $Ecu.RequestId -or $command.Response -ne $Ecu.ResponseId) { continue }
        if ((Test-SingleFrameOnly) -and $command.PayloadBytes -gt 4) { continue }
        if (($command.IdValue -band 0xFF00) -eq 0xF400 -and $null -ne $Ecu.ObdEcu) { continue }
        $reply = Send-ScanUdsRequest $Ecu ([byte[]]@(0x22, ($command.IdValue -shr 8), ($command.IdValue -band 0xFF)))
        if ($reply.Result -eq 'Positive' -and $reply.Message.Length -ge 3) {
            $prefix = '  [{0}] DID {1:X4} ' -f (Get-UdsDisplayName $Ecu), $command.IdValue
            if ((Write-CommandSignals $command (Get-ByteSlice $reply.Message 3) $prefix) -gt 0) { $Ecu.VagDids++ }
        }
        elseif ($reply.Result -eq 'Incomplete') {
            Write-IncompleteReply $Ecu $command.IdValue
        }
    }
}

# Conteggio dei DTC UDS con 19 01 e le maschere 08 (confermati), 04 (in attesa) e 01 (falliti ora): i codici VAG non
# seguono la codifica SAE, quindi se ne riportano solo i conteggi.
function Read-UdsDtcCounts($Ecu) {
    $masks = @(0x08, 0x04, 0x01)
    $fields = @('DtcConfirmed', 'DtcPending', 'DtcFailed')
    for ($index = 0; $index -lt 3; $index++) {
        $reply = Send-ScanUdsRequest $Ecu ([byte[]]@(0x19, 0x01, $masks[$index]))
        if ($reply.Result -ne 'Positive') { continue }
        $count = Get-UdsDtcCount $reply.Message
        if ($null -ne $count) { $Ecu[$fields[$index]] = $count }
    }
}

# Fase 15: letture UDS per centralina, in ordine identificazione, PID standard F4xx, F600/F800, DID VAG OBDb e conteggio
# DTC. Ogni centralina ha un proprio try/catch, così un errore imprevisto salta solo le sue letture; il budget di 60 s
# parte dalla prima lettura.
function Invoke-PhaseUdsRead {
    foreach ($ecu in $script:ScanState.UdsEcus) {
        try {
            $ecu.StartedMs = Get-ScanMilliseconds
            Read-UdsIdentification $ecu
            Read-UdsStandardPids $ecu
            Read-UdsMonitorDeclarations $ecu
            Read-UdsObdbDids $ecu
            Read-UdsDtcCounts $ecu
        }
        catch {
            if ($_.Exception.Message -eq $script:PhaseSkippedMarker -or (Test-FatalScanError $_.Exception)) { throw }
            $ecu.Failure = $_.Exception.Message
            Add-ReportLine ((Get-UdsTag $ecu) + 'errore imprevisto, letture restanti saltate: ' + $_.Exception.Message)
        }
    }
}

# Vero per gli errori dopo cui la scansione non può proseguire su nessuna centralina: porta seriale persa o chiusa,
# interruzione con Ctrl+C.
function Test-FatalScanError([Exception] $Exception) {
    for ($current = $Exception; $null -ne $current; $current = $current.InnerException) {
        if ($current -is [System.IO.IOException] -or $current -is [System.InvalidOperationException] -or
            $current -is [System.UnauthorizedAccessException] -or $current -is [System.Management.Automation.PipelineStoppedException]) { return $true }
    }
    return $false
}

# ============================================================
# Riepilogo e profilo suggerito
# ============================================================

# Riga di riepilogo di una centralina UDS come obdPrintUdsSummary(): decisioni failsafe, DTC (solo conteggi), PID standard
# e DID VAG letti.
function Get-UdsSummaryText($Ecu) {
    $text = ''
    if ($Ecu.Mute) { $text += 'muta dopo la discovery; ' }
    if ($Ecu.Budget) { $text += 'budget esaurito; ' }
    if ($null -ne $Ecu.Failure) { $text += 'errore imprevisto nelle letture; ' }
    $confirmed = if ($null -ne $Ecu.DtcConfirmed) { $Ecu.DtcConfirmed } else { 'non letti' }
    $pending = if ($null -ne $Ecu.DtcPending) { $Ecu.DtcPending } else { 'non letti' }
    $text += "DTC confermati $confirmed, in attesa $pending"
    if ($null -ne $Ecu.DtcFailed) { $text += ", falliti ora $($Ecu.DtcFailed)" }
    return $text + "; PID standard $($Ecu.StdPids), DID VAG $($Ecu.VagDids)"
}

# Elenca gli ID presenti in una bitmap e assenti nell'altra, esclusi quelli di range, con il nome italiano di
# pid_descriptions.h quando c'è.
function Format-PidList([byte[]] $Present, [byte[]] $Absent) {
    $items = New-Object System.Collections.Generic.List[string]
    for ($id = 1; $id -le 0xFF; $id++) {
        if ((Test-RangeId $id) -or -not (Test-BitmapId $Present $id) -or (Test-BitmapId $Absent $id)) { continue }
        $name = ''
        if ($script:ScanTables.PidNames.ContainsKey($id)) { $name = ' ' + $script:ScanTables.PidNames[$id] }
        $items.Add(('{0:X2}{1}' -f $id, $name))
    }
    return $items -join ', '
}

# Fase 16: riepilogo per centralina come obdProbeStepSummary(). Le centraline OBD per ID con PID dichiarati e letti e
# DTC, più gli elenchi dei PID per nome; sotto ciascuna la stessa centralina vista in UDS (7E0/7E8 una volta sola);
# poi le UDS per nome e coppia di ID e le capacità dell'adattatore.
function Add-ScanSummary {
    $scan = $script:ScanState
    $elm = $script:ElmState
    $scan.SummaryWritten = $true
    $interrupted = if ($scan.Stopped) { ', interrotta' } else { '' }
    Add-ReportLine ''
    Add-ReportLine ('=== RIEPILOGO SCANSIONE COMPLETA ({0} s{1}) ===' -f [math]::Floor((Get-ScanMilliseconds) / 1000), $interrupted)
    $noBits = New-Object byte[] 32
    foreach ($ecu in $scan.ObdEcus) {
        $line = (Get-ObdTag $ecu) + "PID dichiarati $(Get-DataIdCount $ecu.Mode01), letti $(Get-DataIdCount $ecu.Mode01Verified)"
        $labels = @('; DTC confermati ', ', in attesa ', ', permanenti ')
        for ($kind = 0; $kind -lt 3; $kind++) {
            $count = if ($ecu.DtcRead[$kind]) { $ecu.DtcCodes[$kind].Count } else { 'non letti' }
            $line += $labels[$kind] + $count
        }
        Add-ReportLine $line
        $lists = @(
            @('      PID letti: ', (Format-PidList $ecu.Mode01Verified $noBits)),
            @('      PID dichiarati e non letti: ', (Format-PidList $ecu.Mode01 $ecu.Mode01Verified)),
            @('      PID letti senza essere dichiarati: ', (Format-PidList $ecu.Mode01Verified $ecu.Mode01)))
        foreach ($entry in $lists) {
            if ($entry[1]) { Add-ReportLine ($entry[0] + $entry[1]) }
        }
        foreach ($uds in $scan.UdsEcus) {
            if ([object]::ReferenceEquals($uds.ObdEcu, $ecu)) { Add-ReportLine ("      UDS $($uds.RequestId)/$($uds.ResponseId): " + (Get-UdsSummaryText $uds)) }
        }
    }
    foreach ($uds in $scan.UdsEcus) {
        if ($null -eq $uds.ObdEcu) { Add-ReportLine ((Get-UdsTag $uds) + (Get-UdsSummaryText $uds)) }
    }
    if ($null -ne $elm) {
        $flowControl = Format-YesNo ($elm.FlowControlAvailable -and $scan.FlowControlTest -ne 'Broken')
        if ($scan.FlowControlTest -eq 'NotRun' -or $scan.FlowControlTest -eq 'Inconclusive') { $flowControl += ' (non verificato)' }
        $stn = 'adattatore non STN'
        if ($elm.StnAvailable -and $elm.StnPairsAvailable) { $stn = 'adattatore STN, coppie di flow control registrate' }
        elseif ($elm.StnAvailable) { $stn = 'adattatore STN, coppie di flow control rifiutate' }
        Add-ReportLine ("Adattatore: richieste fisiche $(Format-YesNo $elm.PhysicalAvailable), flow control verso l'ID di richiesta $flowControl, " +
            "gestione autonoma dei 7F xx 78 $(Format-YesNo $elm.PendingHandled), CAN a 29 bit $(Format-YesNo $elm.ExtendedAvailable); $stn")
    }
}

# Fase 17: profilo motore suggerito come obdProbeStepProfile(), pronto da incollare nel blocco "Profilo vettura" di
# obd_live.h: PID dei candidati a cui il motore ha risposto, esito della prova a 6 PID e coppia del PID 63 se letta.
function Add-ScanProfile {
    $scan = $script:ScanState
    Add-ReportLine ''
    Add-ReportLine '// Profilo suggerito dalla scansione completa: sostituire il blocco "Profilo vettura" di obd_live.h'
    $engine = Find-ObdEcu '7E8'
    if ($null -eq $engine) {
        Add-ReportLine '// Motore 7E8 non trovato: profilo non disponibile'
    }
    else {
        $pids = @($script:ScanTables.ProfileCandidates | Where-Object { Test-BitmapId $engine.Mode01Verified $_ } | ForEach-Object { '0x{0:X2}' -f $_ })
        Add-ReportLine ('static const uint8_t OBD_ENGINE_PIDS[] PROGMEM = {' + ($pids -join ', ') + '};')
        $multiPid = if ($scan.MultiPid6) { 'true' } else { 'false' }
        Add-ReportLine "static constexpr bool OBD_ENGINE_MULTI_PID = $multiPid;"
        if ($null -ne $scan.ReferenceTorqueNm) {
            Add-ReportLine ('// Coppia di riferimento letta (PID 63): ' + (Format-ScanNumber $scan.ReferenceTorqueNm) + ' Nm, confrontarla con TORQUE_MAX_NM di EngineConstants.h')
        }
    }
    Add-ReportLine '=== FINE SCANSIONE COMPLETA ==='
}

# Esegue la scansione completa nell'ordine delle fasi del firmware, poi riepilogo e profilo. Le fasi a 29 bit lasciano
# l'adattatore sul gemello a 29 bit del protocollo e la prima richiesta a 11 bit successiva lo riporta a quello trovato.
function Invoke-FullScan {
    $scan = $script:ScanState
    $scan.Clock.Restart()
    $scan.Started = $true
    Add-ReportLine ''
    Add-ReportLine '=== SCANSIONE COMPLETA: OBD2 e UDS in sola lettura, solo dati interpretabili ==='
    Invoke-ScanPhase 'Centraline OBD' { Invoke-PhaseObdDiscovery }
    Invoke-ScanPhase 'PID supportati' { Invoke-PhaseBitmaps }
    Invoke-ScanPhase 'PID supportati' { Invoke-PhaseScale }
    Invoke-ScanPhase 'PID Mode 01' { Invoke-PhaseMode01 }
    Invoke-ScanPhase 'PID Mode 01' { Invoke-PhaseMultiPid }
    Invoke-ScanPhase 'Freeze frame' { Invoke-PhaseMode02 }
    Invoke-ScanPhase 'Monitor Mode 06' { Invoke-PhaseMode06 }
    Invoke-ScanPhase 'Info veicolo' { Invoke-PhaseMode09 }
    Invoke-ScanPhase 'DTC OBD' { Invoke-PhaseObdDtc }
    Invoke-ScanPhase 'Ricerca centraline UDS' { Invoke-PhaseObd29 }
    Invoke-ScanPhase 'Ricerca centraline UDS' { Invoke-PhaseUdsDiscovery $false }
    Invoke-ScanPhase 'Ricerca centraline UDS' { Invoke-PhaseUdsDiscovery $true }
    Invoke-ScanPhase 'Ricerca centraline UDS' { Invoke-PhaseUdsConfirm }
    Invoke-ScanPhase 'Letture UDS' { Invoke-PhaseFlowControlTest }
    Invoke-ScanPhase 'Letture UDS' { Invoke-PhaseUdsRead }
    Add-ScanSummary
    Add-ScanProfile
}

# Esegue la scansione completa e scrive il report nella cartella indicata anche dopo un errore o un Ctrl+C: in quei casi
# riepilogo e profilo si costruiscono con i dati raccolti fin lì, e la traccia parziale resta in coda.
function Start-PidScan([string] $PortName, [int] $Baud, [string] $OutputDirectory) {
    $ErrorActionPreference = 'Stop'
    $script:TraceLines.Clear()
    $startTime = Get-Date
    $reportPath = Join-Path $OutputDirectory ('pid_scan_{0:yyyyMMdd_HHmmss}.txt' -f $startTime)
    Initialize-ScanState
    Add-ReportLine ('Scansione completa OBD2 e UDS da PC  {0:yyyy-MM-dd HH:mm:ss}' -f $startTime)
    $serialPort = $null
    $exitCode = 1
    $outcome = 'interrupted'
    try {
        Import-ScanTables
        $tables = $script:ScanTables
        Add-ReportLine "Selezione OBDb anno modello $($tables.ModelYear): $($tables.SaeCommands.Count) PID SAE, $($tables.VagCommands.Count) DID VAG; elenco ODIS di $($tables.OdisRows.Count) coppie"
        $serialPort = Open-ElmPort $PortName $Baud
        Initialize-ElmState $serialPort
        Write-Host "Adattatore ELM327 su $($serialPort.PortName) a $($serialPort.BaudRate) baud"
        Initialize-ElmAdapter $serialPort
        $null = Invoke-FullScan
        $outcome = 'completed'
        $exitCode = 0
    }
    catch {
        $outcome = 'failed'
        $script:ScanState.Report.Add("ERRORE: $($_.Exception.Message)")
        Write-Host "ERRORE: $($_.Exception.Message)" -ForegroundColor Red
    }
    finally {
        $scan = $script:ScanState
        if ($scan.Started -and -not $scan.SummaryWritten) {
            try {
                if ($outcome -eq 'interrupted') { Add-ReportLine 'Scansione interrotta su richiesta' }
                $scan.Stopped = $true
                Add-ScanSummary
                Add-ScanProfile
            }
            catch {
                $scan.Report.Add("ERRORE nel riepilogo: $($_.Exception.Message)")
            }
        }
        if ($null -ne $serialPort -and $serialPort.IsOpen) {
            # Come obdTransportRestore(): ATZ toglie filtri, flow control e ATSTFF prima di lasciare l'adattatore; dopo
            # Ctrl+C non si invia nulla, perchè la pipeline è in arresto
            if ($outcome -ne 'interrupted') {
                try { $null = Invoke-ElmCommand $serialPort 'ATZ' 3000 } catch { $script:TraceLines.Add("-- ATZ finale non riuscito: $($_.Exception.Message)") }
            }
            $serialPort.Close()
        }
        $report = $scan.Report
        $report.Add('')
        $report.Add('==================== Traccia TX/RX ====================')
        $report.AddRange($script:TraceLines)
        [System.IO.File]::WriteAllLines($reportPath, $report.ToArray(), (New-Object System.Text.UTF8Encoding $false))
        Write-Host "Report: $reportPath"
    }
    return $exitCode
}

# Lo scan parte solo quando lo script viene eseguito: caricato con dot-source espone soltanto le funzioni
if ($MyInvocation.InvocationName -ne '.') {
    exit (Start-PidScan $Port $BaudRate $PSScriptRoot)
}
