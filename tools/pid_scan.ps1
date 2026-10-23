<#
    Scansione dei PID OBD2 supportati da tutte le ECU del veicolo, tramite un adattatore
    ELM327 su porta COM (Vgate vLinker FS USB: bridge FTDI, 115200 baud di fabbrica).

    Per ogni ECU che risponde sul CAN 11 bit legge le bitmap dei supportati dei Mode 01,
    02, 06 e 09 e il nome dell'ECU (Mode 09 InfoType 0A), poi scrive il report nella
    cartella dello script, con una riga di separazione per ogni ECU e la traccia grezza
    TX/RX in coda. Usa solo servizi di lettura: nessun comando cancella i DTC o modifica
    lo stato delle ECU.

    Uso, a quadro acceso e con FORScan e VCDS chiusi:
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

# Servizi interrogati, nell'ordine in cui compaiono nel report. È un array e non un [ordered]@{}
# perchè l'indicizzatore di OrderedDictionary tratta una chiave intera come posizione
$Services = @(
    @{ Id = 0x01; Label = 'Mode 01 PID supportati' }
    @{ Id = 0x02; Label = 'Mode 02 PID freeze frame supportati' }
    @{ Id = 0x06; Label = 'Mode 06 OBDMID supportati' }
    @{ Id = 0x09; Label = 'Mode 09 InfoType supportati' }
)

# Traccia di tutti gli scambi con l'adattatore, riportata in coda al report
$TraceLines = New-Object System.Collections.Generic.List[string]

# Invia un comando all'ELM327 e ne raccoglie la risposta fino al prompt '>', perchè un
# carattere inviato mentre l'ELM lavora lo interrompe con STOPPED. Ogni scambio finisce nella traccia.
function Invoke-ElmCommand([System.IO.Ports.SerialPort] $SerialPort, [string] $Command, [int] $TimeoutMs = 3000) {
    $SerialPort.DiscardInBuffer()
    $SerialPort.Write($Command + "`r")
    $received = New-Object System.Text.StringBuilder
    $stopwatch = [System.Diagnostics.Stopwatch]::StartNew()
    $promptReceived = $false
    while ($stopwatch.ElapsedMilliseconds -lt $TimeoutMs) {
        if ($SerialPort.BytesToRead -gt 0) {
            [void]$received.Append($SerialPort.ReadExisting())
            if ($received.ToString().Contains('>')) {
                $promptReceived = $true
                break
            }
        }
        else {
            Start-Sleep -Milliseconds 10
        }
    }
    # L'ELM327 può inserire byte 0x00 spuri nell'output, che vanno ignorati (datasheet p.9)
    $text = $received.ToString().Replace([string][char]0, '').Replace('>', '')
    $lines = @($text -split "[`r`n]+" | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne '' })

    $script:TraceLines.Add("TX $Command")
    Write-Verbose "TX $Command"
    foreach ($line in $lines) {
        $script:TraceLines.Add("RX $line")
        Write-Verbose "RX $line"
    }
    if (-not $promptReceived) {
        $script:TraceLines.Add("-- nessun prompt entro $TimeoutMs ms")
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

# Raggruppa per ID CAN le righe stampate con ATH1 ATS1 e riassembla i messaggi ISO-TP: con gli
# header attivi l'ELM327 stampa ogni frame grezzo, PCI e padding compresi, interlacciando le ECU.
function ConvertFrom-CanLines([string[]] $Lines) {
    $messagesById = @{}
    $pendingById = @{}
    foreach ($line in $Lines) {
        if ($line -notmatch '^[0-9A-F]{3}( [0-9A-F])?( [0-9A-F]{2}){1,8}$') {
            continue
        }
        # Il token di una sola cifra dopo l'ID è la DLC, che compare solo se l'adattatore ha AT D1 attivo
        $tokens = @($line.Split(' ') | Where-Object { $_.Length -ne 1 })
        $canId = $tokens[0]
        $frameBytes = @($tokens | Select-Object -Skip 1 | ForEach-Object { [Convert]::ToByte($_, 16) })
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
    return $messagesById
}

# Legge la catena delle bitmap dei supportati di un servizio per tutte le ECU, con una richiesta
# in broadcast per range: il range successivo si chiede solo se almeno un'ECU ha a 1 l'ultimo bit.
function Get-SupportedChain([System.IO.Ports.SerialPort] $SerialPort, [int] $Service, [int] $FrameNumber = -1) {
    $resultById = @{}
    # Il Mode 02 vuole anche il numero del freeze frame, che l'ECU ripete nella risposta prima dei dati
    $dataOffset = if ($FrameNumber -ge 0) { 3 } else { 2 }
    for ($rangeBase = 0x00; $rangeBase -le 0xE0; $rangeBase += 0x20) {
        $request = '{0:X2}{1:X2}' -f $Service, $rangeBase
        if ($FrameNumber -ge 0) {
            $request += '{0:X2}' -f $FrameNumber
        }
        $messagesById = ConvertFrom-CanLines @(Invoke-ElmCommand $SerialPort $request)

        $chainContinues = $false
        foreach ($canId in $messagesById.Keys) {
            foreach ($message in $messagesById[$canId]) {
                if ($message.Count -lt $dataOffset + 4 -or $message[0] -ne ($Service + 0x40) -or $message[1] -ne $rangeBase) {
                    continue
                }
                if ($FrameNumber -ge 0 -and $message[2] -ne $FrameNumber) {
                    continue
                }
                $bitmap = [byte[]]$message[$dataOffset..($dataOffset + 3)]
                if (-not $resultById.ContainsKey($canId)) {
                    $resultById[$canId] = @{
                        Bitmaps = New-Object System.Collections.Generic.List[string]
                        Ids     = New-Object System.Collections.Generic.List[int]
                    }
                }
                $resultById[$canId].Bitmaps.Add(('{0:X2}={1}' -f $rangeBase, (($bitmap | ForEach-Object { '{0:X2}' -f $_ }) -join ' ')))
                # Stessa regola di isPIDSupported() negli sketch: il bit 7 del primo byte è il PID base+1.
                # L'ID 0x100 si scarta perchè l'ultimo bit del range E0 è riservato da SAE J1979
                for ($bitIndex = 0; $bitIndex -lt 32; $bitIndex++) {
                    $supportedId = $rangeBase + $bitIndex + 1
                    if ((($bitmap[$bitIndex -shr 3] -shr (7 - ($bitIndex % 8))) -band 1) -and $supportedId -le 0xFF) {
                        $resultById[$canId].Ids.Add($supportedId)
                    }
                }
                if ($bitmap[3] -band 1) {
                    $chainContinues = $true
                }
                break
            }
        }
        if (-not $chainContinues) {
            break
        }
    }
    return $resultById
}

# Legge il nome delle ECU dal Mode 09 InfoType 0A: 20 byte ASCII dopo 49 0A 01, con sigla e nome
# riempiti di zeri che vanno tolti tutti, perchè SAE J1979 li mette anche dentro la sigla.
function Get-EcuNames([System.IO.Ports.SerialPort] $SerialPort) {
    $namesById = @{}
    $messagesById = ConvertFrom-CanLines @(Invoke-ElmCommand $SerialPort '090A')
    foreach ($canId in $messagesById.Keys) {
        foreach ($message in $messagesById[$canId]) {
            if ($message.Count -ge 4 -and $message[0] -eq 0x49 -and $message[1] -eq 0x0A) {
                $nameBytes = @($message[3..($message.Count - 1)] | Where-Object { $_ -ne 0 })
                $namesById[$canId] = -join ($nameBytes | ForEach-Object { [char]$_ })
            }
        }
    }
    return $namesById
}

# Esegue lo scan completo e scrive il report nella cartella indicata, anche dopo un errore o un
# Ctrl+C, perchè la traccia parziale serve comunque a capire cosa ha risposto l'adattatore.
function Start-PidScan([string] $PortName, [int] $Baud, [string] $OutputDirectory) {
    $script:TraceLines.Clear()
    $startTime = Get-Date
    $reportPath = Join-Path $OutputDirectory ('pid_scan_{0:yyyyMMdd_HHmmss}.txt' -f $startTime)
    $report = New-Object System.Collections.Generic.List[string]
    $report.Add(('Scan PID OBD2  {0:yyyy-MM-dd HH:mm:ss}' -f $startTime))
    $serialPort = $null
    $exitCode = 0
    try {
        $serialPort = Open-ElmPort $PortName $Baud
        Write-Host "Adattatore ELM327 su $($serialPort.PortName) a $($serialPort.BaudRate) baud"

        # ATZ riporta tutte le impostazioni ai default; le altre mostrano header e frame grezzi,
        # tolgono la DLC e fissano la finestra di attesa, così nessuna ECU lenta viene persa
        $null = Invoke-ElmCommand $serialPort 'ATZ' 3000
        foreach ($setupCommand in @('ATE0', 'ATL0', 'ATS1', 'ATH1', 'ATD0', 'ATAT0', 'ATSP0')) {
            $null = Invoke-ElmCommand $serialPort $setupCommand
        }
        $adapterId = @(Invoke-ElmCommand $serialPort 'ATI') -join ' '
        $deviceDescription = @(Invoke-ElmCommand $serialPort 'AT@1') -join ' '
        $report.Add("Adattatore: $($serialPort.PortName) @ $($serialPort.BaudRate) | ATI: $adapterId | AT@1: $deviceDescription")

        # La prima richiesta OBD avvia la ricerca del protocollo, che può richiedere alcuni secondi
        $ecuFound = $false
        for ($attempt = 1; $attempt -le 3 -and -not $ecuFound; $attempt++) {
            Write-Host "Ricerca del protocollo e delle ECU, tentativo $attempt di 3"
            $ecuFound = (ConvertFrom-CanLines @(Invoke-ElmCommand $serialPort '0100' 15000)).Count -gt 0
        }
        $protocolNumber = (@(Invoke-ElmCommand $serialPort 'ATDPN') -join '') -replace '^A', ''
        $protocolDescription = @(Invoke-ElmCommand $serialPort 'ATDP') -join ' '
        $voltage = @(Invoke-ElmCommand $serialPort 'ATRV') -join ' '
        $report.Add("Protocollo: $protocolNumber $protocolDescription | Tensione: $voltage")

        if (-not $ecuFound) {
            if ($protocolNumber -ne '0' -and $protocolNumber -notin @('6', '8')) {
                throw "Il veicolo usa il protocollo $protocolNumber ($protocolDescription): lo script interpreta solo il CAN 11 bit (protocolli 6 e 8)."
            }
            throw 'Nessuna ECU risponde a 0100: controlla che il quadro sia acceso.'
        }

        $chainsByService = @{}
        foreach ($service in $script:Services) {
            Write-Host "Lettura: $($service.Label)"
            $frameNumber = if ($service.Id -eq 0x02) { 0 } else { -1 }
            $chainsByService[$service.Id] = Get-SupportedChain $serialPort $service.Id $frameNumber
        }

        $ecuNames = @{}
        if (@($chainsByService[0x09].Values | Where-Object { $_.Ids -contains 0x0A }).Count -gt 0) {
            Write-Host 'Lettura: nomi delle ECU'
            $ecuNames = Get-EcuNames $serialPort
        }

        $ecuIds = @($chainsByService.Values | ForEach-Object { $_.Keys } | Sort-Object -Unique)
        $report.Add("ECU rispondenti: $($ecuIds -join ', ')")
        foreach ($ecuId in $ecuIds) {
            $ecuName = if ($ecuNames.ContainsKey($ecuId)) { $ecuNames[$ecuId] } else { '(nome non disponibile)' }
            $report.Add('')
            $report.Add("==================== ECU $ecuId | $ecuName ====================")
            foreach ($service in $script:Services) {
                $chain = $chainsByService[$service.Id]
                if (-not $chain.ContainsKey($ecuId)) {
                    $report.Add("$($service.Label): nessuna risposta")
                    continue
                }
                $supportedIds = ($chain[$ecuId].Ids | ForEach-Object { '{0:X2}' -f $_ }) -join ' '
                $report.Add("$($service.Label) ($($chain[$ecuId].Ids.Count)): $supportedIds")
                $report.Add("  bitmap: $($chain[$ecuId].Bitmaps -join ' | ')")
            }
        }
    }
    catch {
        $report.Add("ERRORE: $($_.Exception.Message)")
        Write-Host "ERRORE: $($_.Exception.Message)" -ForegroundColor Red
        $exitCode = 1
    }
    finally {
        if ($null -ne $serialPort -and $serialPort.IsOpen) {
            $serialPort.Close()
        }
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
