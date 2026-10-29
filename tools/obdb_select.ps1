<#
    Seleziona dai signalset OBDb i segnali realmente interpretabili per la scansione completa e ne scrive la tabella
    del firmware, obdb_signals.h nella radice del progetto.

    File prodotti in tools\obdb:
      selection.json   segnali selezionati, letto da pid_scan.ps1 e base della tabella del firmware
      selection.txt    elenco incluso/escluso con il motivo, da rivedere prima di aggiornare il firmware

    Uso:
      .\obdb_select.ps1 -Download        scarica signalset e supporto per anno ai commit fissati, poi seleziona
      .\obdb_select.ps1                  rigenera la selezione dai file già scaricati
      .\obdb_select.ps1 -ModelYear 2023  seleziona per un altro anno modello

    I dati OBDb sono distribuiti con licenza CC-BY-SA-4.0 (vedi tools\obdb\README.md).

    @Mattia Alesi
#>
[CmdletBinding()]
param(
    [int] $ModelYear = 2022,
    [switch] $Download
)

$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'

# Commit fissati: la selezione deve essere riproducibile anche se OBDb cambia
$SaeRepo = 'OBDb/SAEJ1979'
$SaeCommit = '06690177d2d5e11dbf82566297128d673b5a03a7'
$A5Repo = 'OBDb/Audi-A5'
$A5Commit = 'a8214976f2bffda1b6374f4dca1fb6ce9535a1ce'

$ObdbDirectory = Join-Path $PSScriptRoot 'obdb'
$SaePath = Join-Path $ObdbDirectory 'SAEJ1979.json'
$A5Path = Join-Path $ObdbDirectory 'Audi-A5.json'
$SupportPath = Join-Path $ObdbDirectory ("Audi-A5-support-$ModelYear.yaml")
$SelectionJsonPath = Join-Path $ObdbDirectory 'selection.json'
$SelectionTextPath = Join-Path $ObdbDirectory 'selection.txt'

$BooleanUnits = @('noyes', 'yesno', 'offon', 'onoff')
$OpaqueUnits = @('unknown', 'hex', 'normal', '')
# Parole intere: per sottostringa "ratio" troverebbe anche "regeneration" e "concentration"
$ScalarMeaningPattern = '\b(count|counter|number|misfires?|ratio|factor|lambda|gear|satellites?)\b'
# Abbinamenti espliciti dove OBDb non usa lo stesso prefisso per il valore e per il suo bit di supporto (PID 6C, 8F)
$GateOverrides = @{
    'TAC_A_CMD' = 'CTAC_A_SUP'; 'TAC_B_CMD' = 'CTAC_B_SUP'; 'TP_A_REL' = 'RTP_A_SUP'; 'TP_B_REL' = 'RTP_B_SUP'
    'PM11_REGEN' = 'PM11_STAT_SUP'; 'PM11_ACTIVE' = 'PM11_STAT_SUP'; 'PM21_REGEN' = 'PM21_STAT_SUP'; 'PM21_ACTIVE' = 'PM21_STAT_SUP'
}
# Adimensionali VAG senza parola chiave nel nome che l'utente ha scelto di leggere comunque, dopo la revisione
$UserIncludedSignals = @('A5_OIL_P_VLV_SWITCH', 'A5_PISTON_COOLING')
# Refusi dei nomi OBDb che renderebbero il dato fuorviante: nel PID 70 entrambi gli stati dicono "bressure A"
$NameCorrections = @{ 'BP_A' = 'Boost pressure A control status'; 'BP_B' = 'Boost pressure B control status' }
$ScalarStatePattern = '\b(status|mode|state)\b'
# Monitor dei byte C/D di 01/41 definiti da OBDb solo per l'accensione comandata
$SparkMonitorPrefixes = @('EGR', 'HTR', 'O2S', 'ACRF', 'AIR', 'EVAP', 'HCAT', 'CAT')
# PID SAE la cui scala dipende dai massimi dichiarati in 4F (lambda, sonde, MAP) o in 50 (MAF)
$ScaleFrom4F = @('0B', '24', '25', '26', '27', '28', '29', '2A', '2B', '34', '35', '36', '37', '38', '39', '3A', '3B')
$ScaleFrom50 = @('10')

$Utf8NoBom = New-Object System.Text.UTF8Encoding $false

# Scarica un file di testo da GitHub, con qualche nuovo tentativo contro gli errori di rete transitori.
function Get-RemoteText([string] $Url) {
    for ($attempt = 1; $attempt -le 3; $attempt++) {
        try {
            return (Invoke-WebRequest -UseBasicParsing -Uri $Url).Content
        }
        catch {
            if ($attempt -eq 3) { throw "Download fallito: $Url ($($_.Exception.Message))" }
            Start-Sleep -Seconds (2 * $attempt)
        }
    }
}

# URL raw di un file del repository al commit fissato, con i segmenti del percorso codificati.
function Get-RawUrl([string] $Repo, [string] $Commit, [string] $RepoPath) {
    $encoded = ($RepoPath -split '/' | ForEach-Object { [uri]::EscapeDataString($_) }) -join '/'
    return "https://raw.githubusercontent.com/$Repo/$Commit/$encoded"
}

# Scarica ai commit fissati i signalset SAE e A5 e il supporto per anno delle catture A5, da cui vengono le etichette
# "risposto" e "rifiutato" dei DID.
function Save-ObdbSources {
    if (-not (Test-Path $ObdbDirectory)) { New-Item -ItemType Directory -Path $ObdbDirectory | Out-Null }
    [IO.File]::WriteAllText($SaePath, (Get-RemoteText (Get-RawUrl $SaeRepo $SaeCommit 'signalsets/v3/default.json')), $Utf8NoBom)
    [IO.File]::WriteAllText($A5Path, (Get-RemoteText (Get-RawUrl $A5Repo $A5Commit 'signalsets/v3/default.json')), $Utf8NoBom)
    $supportText = Get-RemoteText (Get-RawUrl $A5Repo $A5Commit "tests/test_cases/$ModelYear/command_support.yaml")
    [IO.File]::WriteAllText($SupportPath, $supportText, $Utf8NoBom)
}

# Applica un filtro anni OBDb con la stessa logica del decoder ufficiale (python/can/signals.py): from < to è un
# intervallo chiuso, from >= to vale fuori dall'intervallo, un solo estremo è una semiretta, years si aggiunge in OR.
function Test-YearFilter($Filter, [int] $Year) {
    if ($null -eq $Filter) { return $true }
    $from = $Filter.from
    $to = $Filter.to
    if ($null -ne $from -and $null -ne $to) {
        if ($from -lt $to) {
            if ($Year -ge $from -and $Year -le $to) { return $true }
        }
        elseif ($Year -ge $from -or $Year -le $to) { return $true }
    }
    elseif ($null -ne $to) {
        if ($Year -le $to) { return $true }
    }
    elseif ($null -ne $from) {
        if ($Year -ge $from) { return $true }
    }
    if ($null -ne $Filter.years -and @($Filter.years) -contains $Year) { return $true }
    return $false
}

# Restituisce servizio e identificatore del comando, per esempio 01/0C o 22/2A0C.
function Get-CommandIdentifier($Command) {
    $service = @($Command.cmd.PSObject.Properties)[0].Name
    return @{ Service = $service; Id = [string]$Command.cmd.$service }
}

# Decide se il comando è utilizzabile per l'anno; ritorna il motivo dell'esclusione o $null. Un comando "in debug"
# per l'anno (dbg, oppure dbgfilter che lo include) non è verificato da OBDb e resta fuori.
function Get-CommandExclusion($Command) {
    if ($Command.din) { return "richiede la sessione diagnostica $($Command.din) (din)" }
    if (-not (Test-YearFilter $Command.filter $ModelYear)) { return "non valido per l'anno modello $ModelYear" }
    if ($Command.dbg) { return 'non verificato da OBDb (dbg)' }
    if ($null -ne $Command.dbgfilter -and (Test-YearFilter $Command.dbgfilter $ModelYear)) { return "non verificato da OBDb per il $ModelYear (dbgfilter)" }
    return $null
}

# Classifica un segnale: ritorna @{ Kind; Unit } se interpretabile, altrimenti @{ Reason }. Per lo standard SAE un
# adimensionale ha significato pubblico (lambda, conteggi); per i DID VAG solo se il nome lo dichiara.
function Get-SignalVerdict($Signal, [string] $Source) {
    $name = [string]$Signal.name
    $unit = [string]$Signal.fmt.unit
    # La mappa viene prima del supporto: CIM_SUP è un'enumerazione (accensione comandata o per compressione)
    if ($null -ne $Signal.fmt.map) { return @{ Kind = 'enum'; Unit = $unit } }
    if ($Signal.id -match '_(SUP|ENA)$') { return @{ Kind = 'gate'; Unit = 'noyes' } }
    if ($name -match '\braw\b') { return @{ Reason = 'valore grezzo (raw nel nome)' } }
    if ($unit -eq 'hex' -and $Source -eq 'SAE' -and $Signal.id -eq 'DTCFRZF') { return @{ Kind = 'dtc'; Unit = '' } }
    if ($BooleanUnits -contains $unit) { return @{ Kind = 'boolean'; Unit = $unit } }
    if ($unit -eq 'scalar') {
        if ($name -match $ScalarStatePattern) { return @{ Reason = 'stato senza mappa dei valori' } }
        if ($Source -eq 'SAE') {
            if ($name -match 'NOx concentration') { return @{ Kind = 'value'; Unit = 'ppm' } }
            if ($name -match 'PM mass concentration') { return @{ Kind = 'value'; Unit = 'milligramsPerCubicMeter' } }
            return @{ Kind = 'value'; Unit = 'scalar' }
        }
        if ($name -match $ScalarMeaningPattern -or $UserIncludedSignals -contains $Signal.id) { return @{ Kind = 'value'; Unit = 'scalar' } }
        return @{ Reason = 'adimensionale senza significato pubblico' }
    }
    if ($OpaqueUnits -contains $unit) {
        $shown = if ($unit) { $unit } else { 'assente' }
        return @{ Reason = "unità non interpretabile ($shown)" }
    }
    return @{ Kind = 'value'; Unit = $unit }
}

# Trova il segnale di supporto che abilita quello indicato: X_SUP o X_ENA con il prefisso più lungo che coincide
# con l'identificativo o ne è l'inizio (MAFA_SUP per MAFA, BP_A_ACT_SUP per BP_A_ACT, ACRF_ENA per ACRFCMPL).
function Find-GateSignal($Command, $Signal) {
    if ($Signal.id -match '_(SUP|ENA)$') { return $null }
    if ($GateOverrides.ContainsKey($Signal.id)) { return $GateOverrides[$Signal.id] }
    $best = $null
    $bestLength = 0
    foreach ($candidate in $Command.signals) {
        if ($null -ne $candidate.fmt.map -or $candidate.id -notmatch '^(.+)_(SUP|ENA)$') { continue }
        $prefix = $Matches[1]
        $matchesPrefix = ($Signal.id -eq $prefix) -or $Signal.id.StartsWith($prefix + '_') -or ($Signal.id -eq $prefix + 'CMPL')
        if ($matchesPrefix -and $prefix.Length -gt $bestLength) {
            $best = $candidate.id
            $bestLength = $prefix.Length
        }
    }
    return $best
}

# Converte il formato fmt di un segnale OBDb nei campi di decodifica letti dagli interpreti, con i valori
# predefiniti del decoder ufficiale (bix 0, mul e div 1, add 0, min 0 implicito nel clamp).
function ConvertTo-SignalDefinition($Signal) {
    $format = $Signal.fmt
    $map = $null
    if ($null -ne $format.map) {
        $map = @($format.map.PSObject.Properties | ForEach-Object { [ordered]@{ raw = [int64]$_.Name; text = [string]$_.Value.description; value = [string]$_.Value.value } })
    }
    return [ordered]@{
        id      = $Signal.id
        name    = $Signal.name
        bix     = if ($null -ne $format.bix) { [int]$format.bix } else { 0 }
        len     = [int]$format.len
        sign    = [bool]$format.sign
        blsb    = [bool]$format.blsb
        mul     = if ($null -ne $format.mul) { [double]$format.mul } else { 1.0 }
        div     = if ($null -ne $format.div) { [double]$format.div } else { 1.0 }
        add     = if ($null -ne $format.add) { [double]$format.add } else { 0.0 }
        min     = $format.min
        max     = $format.max
        nullMin = $format.nullmin
        nullMax = $format.nullmax
        map     = $map
    }
}

# Porta in unità metriche i pochi segnali OBDb in miglia o gradi Fahrenheit, cambiando scala e limiti: la
# trasformazione è lineare, quindi clamp e limiti null restano equivalenti. Il min assente vale 0 nell'unità
# originale e va convertito esplicitamente.
function Convert-ImperialSignal($Definition) {
    if ($Definition.unit -eq 'miles') {
        $factor = 1.609344
        $offset = 0.0
        $Definition.unit = 'kilometers'
    }
    elseif ($Definition.unit -eq 'fahrenheit') {
        $factor = 1 / 1.8
        $offset = -32 / 1.8
        $Definition.unit = 'celsius'
    }
    else {
        return
    }
    $Definition.mul = $Definition.mul * $factor
    $Definition.add = $Definition.add * $factor + $offset
    $originalMin = if ($null -ne $Definition.min) { [double]$Definition.min } else { 0.0 }
    $Definition.min = $originalMin * $factor + $offset
    foreach ($field in 'max', 'nullMin', 'nullMax') {
        if ($null -ne $Definition[$field]) { $Definition[$field] = [double]$Definition[$field] * $factor + $offset }
    }
    $Definition.converted = $true
}

# Legge dal file di supporto OBDb dell'anno i comandi risposti e rifiutati dalle vetture catturate.
function Read-CaptureSupport {
    $support = @{ Supported = @{}; Unsupported = @{} }
    if (-not (Test-Path $SupportPath)) { return $support }
    $section = $null
    foreach ($line in [IO.File]::ReadAllLines($SupportPath)) {
        if ($line -match '^supported_commands_by_ecu') { $section = $support.Supported; continue }
        if ($line -match '^unsupported_commands_by_ecu') { $section = $support.Unsupported; continue }
        if ($null -ne $section -and $line -match '^\s+-\s+([0-9A-F]{3,8}\.[0-9A-F]{3,8}\.22[0-9A-F]{4})') { $section[$Matches[1]] = $true }
    }
    return $support
}

# Costruisce la selezione di un signalset: comandi utilizzabili con i soli segnali interpretabili, più l'elenco
# delle esclusioni con il motivo, usato da selection.txt.
function Get-Selection($SignalSet, [string] $Source, $CaptureSupport) {
    $commands = New-Object System.Collections.Generic.List[object]
    $excluded = New-Object System.Collections.Generic.List[object]
    foreach ($command in $SignalSet.commands) {
        $identifier = Get-CommandIdentifier $command
        $commandExclusion = Get-CommandExclusion $command
        $captureKey = '{0}.{1}.22{2}' -f $command.hdr, $command.rax, $identifier.Id
        $capture = $null
        if ($Source -eq 'A5') {
            $capture = if ($CaptureSupport.Supported.ContainsKey($captureKey)) { 'risposto' } elseif ($CaptureSupport.Unsupported.ContainsKey($captureKey)) { 'rifiutato' } else { 'assente' }
        }
        $signals = New-Object System.Collections.Generic.List[object]
        foreach ($signal in $command.signals) {
            $verdict = if ($commandExclusion) { @{ Reason = $commandExclusion } } else { Get-SignalVerdict $signal $Source }
            if ($verdict.Reason) {
                $excluded.Add([pscustomobject]@{ Source = $Source; Hdr = $command.hdr; Service = $identifier.Service; Id = $identifier.Id; Signal = $signal.id; Name = $signal.name; Unit = [string]$signal.fmt.unit; Reason = $verdict.Reason; Capture = $capture })
                continue
            }
            # Prefisso del monitor anche per i nomi senza trattino basso (ACRFCMPL, EVAPCMPL, HCATCMPL)
            $prefix = (($signal.id -replace 'CMPL$', '') -split '_')[0]
            $definition = ConvertTo-SignalDefinition $signal
            $definition.name = if ($Source -eq 'SAE' -and $NameCorrections.ContainsKey($signal.id)) { $NameCorrections[$signal.id] } else { $signal.name }
            $definition.kind = $verdict.Kind
            $definition.unit = $verdict.Unit
            $definition.gate = Find-GateSignal $command $signal
            $definition.sparkOnly = ($identifier.Service -eq '01' -and @('01', '41') -contains $identifier.Id -and $SparkMonitorPrefixes -contains $prefix)
            $definition.scaleDependsOn = $null
            if ($Source -eq 'SAE' -and $ScaleFrom4F -contains $identifier.Id) { $definition.scaleDependsOn = '4F' }
            if ($Source -eq 'SAE' -and $ScaleFrom50 -contains $identifier.Id) { $definition.scaleDependsOn = '50' }
            Convert-ImperialSignal $definition
            $signals.Add($definition)
        }
        # Un comando senza valori da mostrare non si invia: i soli segnali di supporto non dicono nulla
        if (@($signals | Where-Object { $_.kind -ne 'gate' }).Count -eq 0) { continue }
        $commands.Add([ordered]@{
                source  = $Source
                service = $identifier.Service
                id      = $identifier.Id
                hdr     = $command.hdr
                rax     = $command.rax
                fcm1    = [bool]$command.fcm1
                capture = $capture
                signals = $signals.ToArray()
            })
    }
    return @{ Commands = $commands; Excluded = $excluded }
}

# Scrive l'elenco leggibile: riepilogo, validazione sulle catture, casi di confine, inclusi ed esclusi.
function Write-SelectionText($Sae, $A5) {
    $lines = New-Object System.Collections.Generic.List[string]
    $lines.Add("Selezione OBDb dei segnali interpretabili, anno modello $ModelYear")
    $lines.Add("Fonti: $SaeRepo@$($SaeCommit.Substring(0, 7)), $A5Repo@$($A5Commit.Substring(0, 7)); licenza CC-BY-SA-4.0")
    $lines.Add('')
    foreach ($entry in @(@('SAE J1979', $Sae), @('Audi A5', $A5))) {
        $selection = $entry[1]
        $values = @($selection.Commands | ForEach-Object { $_.signals } | Where-Object { $_.kind -ne 'gate' })
        $gates = @($selection.Commands | ForEach-Object { $_.signals } | Where-Object { $_.kind -eq 'gate' })
        $lines.Add(('{0}: {1} segnali inclusi (più {2} di supporto) su {3} comandi' -f $entry[0], $values.Count, $gates.Count, $selection.Commands.Count))
        $selection.Excluded | Group-Object Reason | Sort-Object Count -Descending | ForEach-Object { $lines.Add(('    esclusi {0,4}: {1}' -f $_.Count, $_.Name)) }
    }
    $lines.Add('')
    $lines.Add("Comandi A5 inclusi rispetto alle catture OBDb $ModelYear (vetture miste), per centralina: risposti / rifiutati / assenti / totale")
    $A5.Commands | Group-Object { $_.hdr } | Sort-Object Name | ForEach-Object {
        $group = $_.Group
        $lines.Add(('    {0}: {1} / {2} / {3} / {4}' -f $_.Name, @($group | Where-Object { $_.capture -eq 'risposto' }).Count, @($group | Where-Object { $_.capture -eq 'rifiutato' }).Count, @($group | Where-Object { $_.capture -eq 'assente' }).Count, $group.Count))
    }
    $lines.Add('')
    $lines.Add('Casi di confine: adimensionali VAG risposti nelle catture')
    foreach ($command in ($A5.Commands | Where-Object { $_.capture -eq 'risposto' })) {
        foreach ($signal in ($command.signals | Where-Object { $_.unit -eq 'scalar' })) {
            $lines.Add(('    INCLUSO  {0} 22 {1}  {2}' -f $command.hdr, $command.id, $signal.name))
        }
    }
    foreach ($item in ($A5.Excluded | Where-Object { $_.Capture -eq 'risposto' -and $_.Unit -eq 'scalar' })) {
        $lines.Add(('    ESCLUSO  {0} 22 {1}  {2}  ({3})' -f $item.Hdr, $item.Id, $item.Name, $item.Reason))
    }
    $lines.Add('')
    $lines.Add('Controllo: valori senza bit di supporto abbinato in PID che ne hanno (da verificare uno per uno)')
    foreach ($command in $Sae.Commands) {
        if (@($command.signals | Where-Object { $_.kind -eq 'gate' }).Count -eq 0) { continue }
        foreach ($signal in ($command.signals | Where-Object { $_.kind -ne 'gate' -and -not $_.gate })) {
            $lines.Add(('    PID {0}  {1,-18} {2}' -f $command.id, $signal.id, $signal.name))
        }
    }
    $lines.Add("Nomi corretti rispetto a OBDb: $(($NameCorrections.Keys | Sort-Object) -join ', ')")
    $lines.Add("Inclusi per scelta dopo la revisione: $($UserIncludedSignals -join ', ')")
    foreach ($entry in @(@('SAE', $Sae), @('A5', $A5))) {
        $lines.Add('')
        $lines.Add("==================== INCLUSI $($entry[0]) ====================")
        foreach ($command in $entry[1].Commands) {
            $header = if ($entry[0] -eq 'SAE') { 'Mode {0} PID {1}' -f $command.service, $command.id } else { '{0}/{1} 22 {2}  [catture: {3}]' -f $command.hdr, $command.rax, $command.id, $command.capture }
            $lines.Add($header)
            foreach ($signal in $command.signals) {
                $notes = @()
                if ($signal.kind -eq 'gate') { $notes += 'supporto' }
                if ($signal.gate) { $notes += "mostrato se $($signal.gate)" }
                if ($signal.sparkOnly) { $notes += 'solo accensione comandata' }
                if ($signal.scaleDependsOn) { $notes += "scala da $($signal.scaleDependsOn)" }
                $unit = if ($signal.unit) { $signal.unit } else { $signal.kind }
                $suffix = if ($notes.Count) { '  (' + ($notes -join ', ') + ')' } else { '' }
                $lines.Add(('    {0,-22} {1} [{2}]{3}' -f $signal.id, $signal.name, $unit, $suffix))
            }
        }
        $lines.Add('')
        $lines.Add("==================== ESCLUSI $($entry[0]) ====================")
        foreach ($item in $entry[1].Excluded) {
            $where = if ($entry[0] -eq 'SAE') { 'PID {0}' -f $item.Id } else { '{0} 22 {1}' -f $item.Hdr, $item.Id }
            $lines.Add(('    {0,-14} {1,-22} {2}  ({3})' -f $where, $item.Signal, $item.Name, $item.Reason))
        }
    }
    [IO.File]::WriteAllLines($SelectionTextPath, $lines.ToArray(), $Utf8NoBom)
}

# Simboli italiani delle unità della selezione: un'unità nuova ferma il generatore finchè non viene aggiunta qui
$UnitSymbols = @{
    '' = ''; 'scalar' = ''; 'percent' = '%'; 'celsius' = '°C'; 'seconds' = 's'; 'kilopascal' = 'kPa'
    'kilowattHours' = 'kWh'; 'volts' = 'V'; 'newtonMeters' = 'Nm'; 'kilometers' = 'km'; 'amps' = 'A'; 'ppm' = 'ppm'
    'bars' = 'bar'; 'rpm' = 'rpm'; 'kilowatts' = 'kW'; 'hours' = 'h'; 'gramsPerSecond' = 'g/s'; 'milliamps' = 'mA'
    'kilometersPerHour' = 'km/h'; 'degrees' = '°'; 'milliseconds' = 'ms'; 'ampereHours' = 'Ah'; 'kilogramsPerHour' = 'kg/h'
    'gramsPerLiter' = 'g/L'; 'minutes' = 'min'; 'millimeters' = 'mm'; 'litersPerHour' = 'L/h'; 'centimeters' = 'cm'
    'millivolts' = 'mV'; 'milligramsPerCubicMeter' = 'mg/m³'; 'liters' = 'L'; 'metersPerSecondSquared' = 'm/s²'
    'milliohms' = 'mΩ'; 'milligramsPerStroke' = 'mg/corsa'; 'radians' = 'rad'
}
# Testi dei booleani per il bit a 0 e a 1: la prima parola dell'unità OBDb è il valore del bit a 0
$BooleanTexts = @{ 'noyes' = @('no', 'sì'); 'yesno' = @('sì', 'no'); 'offon' = @('spento', 'acceso'); 'onoff' = @('acceso', 'spento') }
$FirmwareTablePath = Join-Path (Split-Path $PSScriptRoot -Parent) 'obdb_signals.h'
$Invariant = [System.Globalization.CultureInfo]::InvariantCulture

# Scrive un testo come contenuto di letterale C con i byte UTF-8: l'ASCII stampabile resta tale, il resto diventa
# escape ottale a tre cifre, che a differenza di \x non assorbe le cifre seguenti; anche ? si scappa, per i trigrafi.
function ConvertTo-CLiteral([string] $Text) {
    $builder = New-Object System.Text.StringBuilder
    foreach ($byte in $Utf8NoBom.GetBytes($Text)) {
        if ($byte -eq 0x22 -or $byte -eq 0x5C) { [void]$builder.Append('\').Append([char]$byte) }
        elseif ($byte -ge 0x20 -and $byte -le 0x7E -and $byte -ne 0x3F) { [void]$builder.Append([char]$byte) }
        else { [void]$builder.Append('\').Append([Convert]::ToString($byte, 8).PadLeft(3, '0')) }
    }
    return $builder.ToString()
}

# Scrive un double come letterale C con precisione completa; un limite assente diventa NAN.
function ConvertTo-CDouble($Value) {
    if ($null -eq $Value) { return 'NAN' }
    $text = ([double]$Value).ToString('R', $Invariant)
    if ($text -notmatch '[.eE]') { $text += '.0' }
    return $text
}

# Aggiunge un testo al blob dei testi senza duplicati e ne ritorna l'offset in byte.
function Add-BlobText($Blob, [string] $Text) {
    if ($Blob.Offsets.ContainsKey($Text)) { return $Blob.Offsets[$Text] }
    $offset = $Blob.Length
    $Blob.Offsets[$Text] = $offset
    $Blob.Items.Add($Text)
    $Blob.Length += $Utf8NoBom.GetByteCount($Text) + 1
    return $offset
}

# Restituisce l'indice di una voce in una tabella deduplicata per chiave, aggiungendola se nuova, entro il massimo
# rappresentabile nel campo del descrittore.
function Get-TableIndex($Table, [string] $Key, $Entry, [int] $Maximum, [string] $What) {
    if ($Table.Index.ContainsKey($Key)) { return $Table.Index[$Key] }
    if ($Table.Items.Count -gt $Maximum) { throw "Troppe voci in $What (massimo $($Maximum + 1)): va allargato il campo del descrittore." }
    $Table.Index[$Key] = $Table.Items.Count
    $Table.Items.Add($Entry)
    return $Table.Index[$Key]
}

# Nuova tabella deduplicata con chiavi sensibili alle maiuscole (le hashtable di PowerShell non lo sono).
function New-IndexedTable {
    return @{ Index = (New-Object 'System.Collections.Generic.Dictionary[string,int]'); Items = (New-Object System.Collections.Generic.List[object]) }
}

# Calcola i campi del descrittore C di un segnale (flag, scala, limiti) a partire dalla sua definizione.
function Get-SignalFlags($Definition) {
    $flags = 0
    if ($Definition.sign) { $flags = $flags -bor 0x01 }
    if ($Definition.blsb) { $flags = $flags -bor 0x02 }
    if ($Definition.sparkOnly) { $flags = $flags -bor 0x04 }
    switch ($Definition.kind) {
        'boolean' { $flags = $flags -bor 0x10 }
        'enum' { $flags = $flags -bor 0x20 }
        'dtc' { $flags = $flags -bor 0x30 }
        'gate' { $flags = $flags -bor 0x40 }
    }
    return $flags
}

# Chiavi testuali delle terne di scala e dei limiti, usate per la deduplica e per scrivere le tabelle.
function Get-ScaleText($Definition) {
    return '{{{0}, {1}, {2}}}' -f (ConvertTo-CDouble $Definition.mul), (ConvertTo-CDouble $Definition.div), (ConvertTo-CDouble $Definition.add)
}
function Get-LimitText($Definition) {
    $min = if ($null -ne $Definition.min) { $Definition.min } else { 0.0 }
    return '{{{0}, {1}, {2}, {3}}}' -f (ConvertTo-CDouble $min), (ConvertTo-CDouble $Definition.max), (ConvertTo-CDouble $Definition.nullMin), (ConvertTo-CDouble $Definition.nullMax)
}

# Scrive obdb_signals.h, la tabella PROGMEM dei comandi e dei segnali selezionati letta dalla scansione del firmware.
function Write-FirmwareTables($Commands) {
    $blob = @{ Offsets = (New-Object 'System.Collections.Generic.Dictionary[string,int]'); Items = (New-Object System.Collections.Generic.List[string]); Length = 0 }
    $units = New-IndexedTable
    $scales = New-IndexedTable
    $limits = New-IndexedTable
    $enums = New-Object System.Collections.Generic.List[string]
    $enumEntries = New-Object System.Collections.Generic.List[string]
    $signalLines = New-Object System.Collections.Generic.List[string]
    $commandLines = New-Object System.Collections.Generic.List[string]

    foreach ($command in $Commands) {
        $localIndex = @{}
        for ($i = 0; $i -lt $command.signals.Count; $i++) { $localIndex[$command.signals[$i].id] = $i }
        $cimIndex = 0xFF
        foreach ($cimId in 'CIM_SUP', 'CIM_SUP_FLAG') { if ($localIndex.ContainsKey($cimId)) { $cimIndex = $localIndex[$cimId] } }
        $firstSignal = $signalLines.Count
        foreach ($signal in $command.signals) {
            $unitKey = if ($signal.kind -eq 'gate') { 'noyes' } else { [string]$signal.unit }
            if ($BooleanTexts.ContainsKey($unitKey)) {
                $unitEntry = '{{{0}, {1}, {2}}}' -f (Add-BlobText $blob ''), (Add-BlobText $blob $BooleanTexts[$unitKey][0]), (Add-BlobText $blob $BooleanTexts[$unitKey][1])
            }
            elseif ($UnitSymbols.ContainsKey($unitKey)) {
                $unitEntry = '{{{0}, 0xFFFF, 0xFFFF}}' -f (Add-BlobText $blob $UnitSymbols[$unitKey])
            }
            else {
                throw "Unità senza simbolo: '$unitKey' ($($signal.id)). Aggiungerla a `$UnitSymbols."
            }
            $unitIndex = Get-TableIndex $units $unitKey $unitEntry 254 'unità'
            $scaleText = Get-ScaleText $signal
            $scaleIndex = Get-TableIndex $scales $scaleText $scaleText 254 'scale'
            $limitText = Get-LimitText $signal
            $limitIndex = Get-TableIndex $limits $limitText $limitText 254 'limiti'
            $enumIndex = 0xFF
            if ($null -ne $signal.map) {
                if ($enums.Count -ge 255 -or $signal.map.Count -gt 255) { throw "Enumerazioni oltre il limite del descrittore ($($signal.id))." }
                $enumIndex = $enums.Count
                $enums.Add(('{{{0}, {1}, 0}}' -f $enumEntries.Count, $signal.map.Count))
                foreach ($entry in $signal.map) {
                    if ($entry.raw -lt 0 -or $entry.raw -gt 65535) { throw "Valore di enumerazione fuori da 16 bit ($($signal.id) = $($entry.raw))." }
                    $text = if ($entry.text) { $entry.text } else { $entry.value }
                    $enumEntries.Add(('{{{0}, {1}}}' -f $entry.raw, (Add-BlobText $blob $text)))
                }
            }
            $gateIndex = if ($signal.gate -and $localIndex.ContainsKey($signal.gate)) { $localIndex[$signal.gate] } else { 0xFF }
            $nameOffset = if ($signal.kind -eq 'gate') { 0xFFFF } else { Add-BlobText $blob $signal.name }
            $signalLines.Add(('{{{0}, {1}, {2}, 0x{3:X2}, {4}, {5}, {6}, {7}, {8}, 0}}' -f $signal.bix, $nameOffset, $signal.len, (Get-SignalFlags $signal), $unitIndex, $scaleIndex, $limitIndex, $enumIndex, $gateIndex))
        }
        $isSae = $command.source -eq 'SAE'
        $header = if ($isSae) { 0x7DF } else { [Convert]::ToUInt32($command.hdr, 16) }
        $responseId = if ($isSae) { 0 } else { [Convert]::ToUInt32($command.rax, 16) }
        $flags = 0
        if ($isSae) { $flags = $flags -bor 0x01 }
        if ($command.capture -eq 'risposto') { $flags = $flags -bor 0x02 }
        if ($command.capture -eq 'rifiutato') { $flags = $flags -bor 0x04 }
        $commandLines.Add(('{{0x{0:X}, 0x{1:X}, 0x{2:X}, {3}, 0x{4}, {5}, 0x{6:X2}, {7}, {{0, 0}}}}' -f $header, $responseId, [Convert]::ToUInt16($command.id, 16), $firstSignal, $command.service, $command.signals.Count, $flags, $cimIndex))
    }
    if ($blob.Length -gt 65535) { throw "Blob dei testi oltre i 64 KB indirizzabili dagli offset a 16 bit ($($blob.Length) byte)." }

    $out = New-Object System.Collections.Generic.List[string]
    $out.Add('/**')
    $out.Add(" * Tabelle dei segnali OBDb interpretabili per la scansione completa, anno modello $ModelYear. File generato da")
    $out.Add(" * tools/obdb_select.ps1 dai signalset $SaeRepo@$($SaeCommit.Substring(0, 7)) e $A5Repo@$($A5Commit.Substring(0, 7)) (licenza")
    $out.Add(' * CC-BY-SA-4.0, vedi tools/obdb/README.md): non va modificato a mano, si rigenera con lo script.')
    $out.Add(' *')
    $out.Add(' * Usato da: obd_scan.h')
    $out.Add(' *')
    $out.Add(' * @Mattia Alesi')
    $out.Add(' */')
    $out.Add('')
    $out.Add('#ifndef OBDB_SIGNALS_H')
    $out.Add('#define OBDB_SIGNALS_H')
    $out.Add('')
    $out.Add('#include "obd_proto.h"')
    $out.Add('')
    $out.Add("static constexpr uint16_t OBDB_MODEL_YEAR = $ModelYear;")
    $out.Add("static constexpr uint16_t OBDB_COMMAND_COUNT = $($commandLines.Count);")
    $out.Add("static constexpr uint16_t OBDB_SIGNAL_COUNT = $($signalLines.Count);")
    $out.Add('')
    $out.Add('// Testi terminati da zero, indirizzati dagli offset dei descrittori')
    $out.Add('static const char OBDB_TEXT[] OBD_PROGMEM =')
    for ($i = 0; $i -lt $blob.Items.Count; $i++) {
        $terminator = if ($i -eq $blob.Items.Count - 1) { ';' } else { '' }
        $out.Add(("`t""{0}\000""{1}" -f (ConvertTo-CLiteral $blob.Items[$i]), $terminator))
    }
    $out.Add('static_assert(sizeof(OBDB_TEXT) <= 65535, "Gli offset a 16 bit non indirizzano oltre 64 KB di testi");')
    foreach ($table in @(
            @('ObdbUnitDef', 'OBDB_UNITS', $units.Items),
            @('ObdbScale', 'OBDB_SCALES', $scales.Items),
            @('ObdbLimit', 'OBDB_LIMITS', $limits.Items),
            @('ObdbEnumDef', 'OBDB_ENUMS', $enums),
            @('ObdbEnumEntry', 'OBDB_ENUM_ENTRIES', $enumEntries),
            @('ObdbSignalDef', 'OBDB_SIGNALS', $signalLines),
            @('ObdbCommandDef', 'OBDB_COMMANDS', $commandLines))) {
        $out.Add('')
        $out.Add(('static const {0} {1}[] OBD_PROGMEM = {{' -f $table[0], $table[1]))
        $items = $table[2].ToArray()
        for ($i = 0; $i -lt $items.Count; $i++) {
            $separator = if ($i -eq $items.Count - 1) { '' } else { ',' }
            $out.Add("`t$($items[$i])$separator")
        }
        $out.Add('};')
    }
    $out.Add('')
    $out.Add('#endif')
    [IO.File]::WriteAllLines($FirmwareTablePath, $out.ToArray(), $Utf8NoBom)
    Write-Host ("Tabella firmware: {0} comandi, {1} segnali, {2} byte di testi, {3} scale, {4} limiti, {5} unità -> {6}" -f $commandLines.Count, $signalLines.Count, $blob.Length, $scales.Items.Count, $limits.Items.Count, $units.Items.Count, $FirmwareTablePath)
}

if ($Download) { Save-ObdbSources }
foreach ($path in @($SaePath, $A5Path)) {
    if (-not (Test-Path $path)) { throw "Manca $($path): eseguire prima con -Download." }
}
$saeSet = [IO.File]::ReadAllText($SaePath) | ConvertFrom-Json
$a5Set = [IO.File]::ReadAllText($A5Path) | ConvertFrom-Json
$captureSupport = Read-CaptureSupport
$saeSelection = Get-Selection $saeSet 'SAE' $captureSupport
$a5Selection = Get-Selection $a5Set 'A5' $captureSupport

$unitSymbolsForJson = [ordered]@{}
# La chiave vuota non è un nome di proprietà valido per ConvertFrom-Json di PowerShell 5.1
foreach ($unitName in ($UnitSymbols.Keys | Where-Object { $_ } | Sort-Object)) { $unitSymbolsForJson[$unitName] = $UnitSymbols[$unitName] }
$document = [ordered]@{
    modelYear = $ModelYear
    sources   = [ordered]@{ sae = "$SaeRepo@$SaeCommit"; a5 = "$A5Repo@$A5Commit"; license = 'CC-BY-SA-4.0' }
    # Simboli delle unità e testi dei booleani del firmware, così pid_scan.ps1 stampa i valori allo stesso modo
    units     = $unitSymbolsForJson
    booleans  = $BooleanTexts
    # In PowerShell 5.1 @() su una List di dizionari ordinati fallisce ("Tipi di argomento non corrispondenti")
    commands  = $saeSelection.Commands.ToArray() + $a5Selection.Commands.ToArray()
}
[IO.File]::WriteAllText($SelectionJsonPath, ($document | ConvertTo-Json -Depth 10 -Compress), $Utf8NoBom)
Write-SelectionText $saeSelection $a5Selection
Write-FirmwareTables $document.commands
Write-Host "Selezione: $SelectionJsonPath"
Write-Host "Elenco da rivedere: $SelectionTextPath"
