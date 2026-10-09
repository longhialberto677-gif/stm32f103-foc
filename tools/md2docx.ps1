<#
.SYNOPSIS
    把"变更记录"里的 Markdown 转换成图片内嵌的 Word 文档（.docx）。

.DESCRIPTION
    直接驱动 Word COM 逐段构建文档，不用 HTML 中转 —— 因为 Word 解析 HTML 里的
    本地图片不可靠（中文/带空格路径直接失败，即使改成 ASCII 名也可能只留空框）。
    这里用 InlineShapes.AddPicture(..., LinkToFile=$false, SaveWithDocument=$true)，
    图片必定内嵌进 .docx，不再依赖外部文件。

    支持的 Markdown 子集：标题(#/##/###)、表格、围栏代码块、有序/无序列表、
    引用块、分隔线、**粗体**、`行内代码`、![图注](图片路径)。

.EXAMPLE
    pwsh tools/md2docx.ps1 -Md "变更记录/01-中断占用率饱和分析/01-中断占用率饱和分析.md"
    pwsh tools/md2docx.ps1 -Md <md路径> -Docx <输出路径> -ImageWidthCm 16
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$Md,
    [string]$Docx,
    [double]$ImageWidthCm = 16.0
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression.FileSystem

$BodyFont = '微软雅黑'
$MonoFont = 'Consolas'
$BodySize = 10.5
$TblSize = 9.0
$CodeSize = 9.0

# ============================ Markdown 解析 ============================
# 把 md 拆成一串"块"，每块是一个 hashtable：@{Kind='h1|p|code|ul|ol|quote|hr|img|table'; ...}
function Parse-Md([string]$text) {
    # 注意：所有跨函数共享的状态都必须放在"引用类型"里（泛型 List / 哈希表字段），
    # 用 .Add()/.Clear() 改内容。绝不能在嵌套函数里写 "$x = @()" —— PowerShell
    # 的变量赋值是写时复制的，那只会在嵌套函数自己的作用域里建一个新变量，
    # 父作用域的缓冲区永远清不掉，结果是缓冲区无限累积、内容被反复重复输出。
    $blocks = New-Object System.Collections.Generic.List[object]
    $st = @{
        Table = New-Object System.Collections.Generic.List[string]
        Quote = New-Object System.Collections.Generic.List[string]
        List  = $null      # 当前正在收集的列表块（哈希表，引用类型）
    }
    $lines = $text -split "`r?`n"
    $inCode = $false; $buf = New-Object System.Collections.Generic.List[string]

    function FlushTable($blocks, $st) {
        if ($st.Table.Count -eq 0) { return }
        $rows = New-Object System.Collections.Generic.List[object]
        foreach ($row in $st.Table) {
            if ($row -match '^\s*\|[\s\-\|:]+\|\s*$') { continue }
            $cells = @($row.Trim().Trim('|') -split '\|' | ForEach-Object {
                # 表格单元格里去掉 md 标记，但整格粗体的保留粗体意图
                $c = $_.Trim()
                if ($c -match '^\*\*(.+)\*\*$') { @{ Text = $matches[1]; Bold = $true } }
                else { @{ Text = ($c -replace '\*\*', '' -replace '`', ''); Bold = $false } }
            })
            $rows.Add($cells)
        }
        $blocks.Add(@{ Kind = 'table'; Rows = $rows })
        $st.Table.Clear()
    }
    function FlushQuote($blocks, $st) {
        if ($st.Quote.Count -eq 0) { return }
        $blocks.Add(@{ Kind = 'quote'; Lines = @($st.Quote) })
        $st.Quote.Clear()
    }
    function FlushAll($blocks, $st) { FlushTable $blocks $st; FlushQuote $blocks $st; $st.List = $null }

    foreach ($line in $lines) {
        if ($line -match '^\s*```') {
            if ($inCode) { $blocks.Add(@{ Kind = 'code'; Lines = @($buf) }); $buf.Clear(); $inCode = $false }
            else { FlushAll $blocks $st; $inCode = $true }
            continue
        }
        if ($inCode) { $buf.Add($line); continue }

        if ($line.Trim() -eq '') { FlushAll $blocks $st; continue }
        if ($line -match '^\s*-{3,}\s*$') { FlushAll $blocks $st; $blocks.Add(@{ Kind = 'hr' }); continue }

        if ($line -match '^\s*!\[(.*?)\]\((.*?)\)\s*$') {
            FlushAll $blocks $st; $blocks.Add(@{ Kind = 'img'; Cap = $matches[1]; Src = $matches[2] }); continue
        }
        if ($line -match '^\s*\|') { FlushQuote $blocks $st; $st.List = $null; $st.Table.Add($line); continue }
        FlushTable $blocks $st

        if ($line -match '^\s*>\s?(.*)$') { $st.List = $null; $st.Quote.Add($matches[1]); continue }
        FlushQuote $blocks $st

        if ($line -match '^(#{1,6})\s+(.*)$') {
            FlushAll $blocks $st; $blocks.Add(@{ Kind = "h$($matches[1].Length)"; Text = $matches[2] }); continue
        }
        if ($line -match '^\s*[-*]\s+(.*)$') {
            if (-not $st.List -or $st.List.Kind -ne 'ul') {
                $st.List = @{ Kind = 'ul'; Items = New-Object System.Collections.Generic.List[string] }
                $blocks.Add($st.List)
            }
            $st.List.Items.Add($matches[1]); continue
        }
        if ($line -match '^\s*(\d+)\.\s+(.*)$') {
            if (-not $st.List -or $st.List.Kind -ne 'ol') {
                $st.List = @{ Kind = 'ol'; Items = New-Object System.Collections.Generic.List[string] }
                $blocks.Add($st.List)
            }
            $st.List.Items.Add("$($matches[1]). $($matches[2])"); continue
        }
        $st.List = $null

        $blocks.Add(@{ Kind = 'p'; Text = $line })
    }
    if ($inCode -and $buf.Count -gt 0) { $blocks.Add(@{ Kind = 'code'; Lines = @($buf) }) }
    FlushAll $blocks $st
    return $blocks
}

# 把一段文本按 **粗体** 和 `行内代码` 拆成若干 run
function Split-Runs([string]$text) {
    $runs = @()
    foreach ($part in [regex]::Split($text, '(\*\*.+?\*\*|`[^`]+`)')) {
        if ($part -eq '') { continue }
        if ($part -match '^\*\*(.+)\*\*$')      { $runs += @{ Text = $matches[1]; Kind = 'bold' } }
        elseif ($part -match '^`(.+)`$')        { $runs += @{ Text = $matches[1]; Kind = 'code' } }
        else                                    { $runs += @{ Text = $part;       Kind = 'norm' } }
    }
    return $runs
}

# ============================ 路径准备 ============================
$MdPath = (Resolve-Path -LiteralPath $Md).Path
if (-not $Docx) { $Docx = [System.IO.Path]::ChangeExtension($MdPath, '.docx') }
$DocxPath = [System.IO.Path]::GetFullPath($Docx)
$mdDir = Split-Path $MdPath -Parent

Write-Host "源文件 : $MdPath"
Write-Host "输出   : $DocxPath"

$mdText = [System.IO.File]::ReadAllText($MdPath, [System.Text.UTF8Encoding]::new($false))
$blocks = Parse-Md $mdText
$imgBlocks = @($blocks | Where-Object { $_.Kind -eq 'img' })
$byKind = ($blocks | Group-Object Kind | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Count)" }) -join ', '
Write-Host ("解析   : {0} 个块  [{1}]" -f $blocks.Count, $byKind)
if ($blocks.Count -gt 800) { throw "解析出 $($blocks.Count) 个块，明显异常（预期 100~200），先检查 Parse-Md" }

# ============================ 驱动 Word ============================
$word = $null; $doc = $null; $sel = $null
try {
    $word = New-Object -ComObject Word.Application
    $word.Visible = $false
    $word.DisplayAlerts = 0
    $doc = $word.Documents.Add()
    $sel = $word.Selection
    $doc.PageSetup.PageWidth    = $word.CentimetersToPoints(21.0)
    $doc.PageSetup.PageHeight   = $word.CentimetersToPoints(29.7)
    $doc.PageSetup.TopMargin    = $word.CentimetersToPoints(2.2)
    $doc.PageSetup.BottomMargin = $word.CentimetersToPoints(2.2)
    $doc.PageSetup.LeftMargin   = $word.CentimetersToPoints(2.0)
    $doc.PageSetup.RightMargin  = $word.CentimetersToPoints(2.0)

    $Normal = $doc.Styles.Item(-1)     # wdStyleNormal
    $H1 = $doc.Styles.Item(-2); $H2 = $doc.Styles.Item(-3); $H3 = $doc.Styles.Item(-4)

    function Write-Para([string]$text, [string]$style = 'norm', [double]$size = 0, [string]$font = '', [bool]$center = $false) {
        $sel.Style = $Normal
        if ($size -gt 0) { $sel.Font.Size = $size }
        if ($font) { $sel.Font.Name = $font }
        $sel.ParagraphFormat.Alignment = if ($center) { 1 } else { 0 }   # 0=左 1=居中
        foreach ($r in (Split-Runs $text)) {
            switch ($r.Kind) {
                'bold' { $sel.Font.Bold = $true; $sel.TypeText($r.Text); $sel.Font.Bold = $false }
                'code' { $sel.Font.Name = $MonoFont; $sel.Font.Size = $CodeSize; $sel.TypeText($r.Text); $sel.Font.Name = $BodyFont; $sel.Font.Size = $BodySize }
                default { $sel.TypeText($r.Text) }
            }
        }
        $sel.TypeParagraph()
        # 复位
        $sel.Font.Bold = $false; $sel.Font.Name = $BodyFont; $sel.Font.Size = $BodySize
        $sel.ParagraphFormat.Alignment = 0
    }

    $bi = 0
    foreach ($b in $blocks) {
        $bi++
        if ($bi % 25 -eq 0) { Write-Host ("  ...已写入 {0}/{1} 块（当前 {2}）" -f $bi, $blocks.Count, $b.Kind) }
        switch ($b.Kind) {
            'h1' { $sel.Style = $H1; $sel.TypeText(($b.Text -replace '\*\*', '' -replace '`', '')); $sel.TypeParagraph(); $sel.Style = $Normal }
            'h2' { $sel.Style = $H2; $sel.TypeText(($b.Text -replace '\*\*', '' -replace '`', '')); $sel.TypeParagraph(); $sel.Style = $Normal }
            'h3' { $sel.Style = $H3; $sel.TypeText(($b.Text -replace '\*\*', '' -replace '`', '')); $sel.TypeParagraph(); $sel.Style = $Normal }
            'hr' {
                $sel.Style = $Normal
                $sel.ParagraphFormat.Alignment = 1
                $sel.Font.Size = 6
                $sel.TypeText('───────────────')
                $sel.TypeParagraph()
                $sel.Font.Size = $BodySize; $sel.ParagraphFormat.Alignment = 0
            }
            'p'    { Write-Para $b.Text }
            'quote' { foreach ($l in $b.Lines) { Write-Para $l 'norm' 9.0 } }
            'code' {
                $sel.Style = $Normal; $sel.Font.Name = $MonoFont; $sel.Font.Size = $CodeSize
                foreach ($l in $b.Lines) { $sel.TypeText($l); $sel.TypeParagraph() }
                $sel.Font.Name = $BodyFont; $sel.Font.Size = $BodySize
            }
            'ul' { foreach ($i in $b.Items) { Write-Para ("· " + $i) } }
            'ol' { foreach ($i in $b.Items) { Write-Para $i } }
            'img' {
                $abs = Join-Path $mdDir $b.Src
                if (-not (Test-Path -LiteralPath $abs)) { throw "图片不存在：$abs" }
                $pic = $doc.InlineShapes.AddPicture($abs, $false, $true, $sel.Range)   # 内嵌
                $pic.Width = $word.CentimetersToPoints($ImageWidthCm)
                $sel.Style = $Normal
                $sel.EndKey(6) | Out-Null          # 6 = wdStory，移到文末
                $sel.TypeParagraph()
                Write-Para $b.Cap 'norm' 9.0 $BodyFont $true
            }
            'table' {
                $rows = $b.Rows.Count
                $cols = ($b.Rows | ForEach-Object { $_.Count } | Measure-Object -Maximum).Maximum
                $tbl = $doc.Tables.Add($sel.Range, $rows, $cols)
                $tbl.Borders.Enable = $true
                $tbl.Range.Font.Size = $TblSize
                $tbl.Range.Font.Name = $BodyFont
                for ($r = 1; $r -le $rows; $r++) {
                    $cells = $b.Rows[$r - 1]
                    for ($c = 1; $c -le $cols; $c++) {
                        $cell = $tbl.Cell($r, $c)
                        if ($c -le $cells.Count) { $cell.Range.Text = $cells[$c - 1].Text } else { $cell.Range.Text = '' }
                    }
                }
                $tbl.Rows.Item(1).Range.Font.Bold = $true
                $tbl.Rows.Item(1).Shading.BackgroundPatternColor = 15132390
                $tbl.AutoFitBehavior(2) | Out-Null   # 2 = wdAutoFitWindow
                $sel.EndKey(6) | Out-Null
                $sel.TypeParagraph()
            }
        }
    }

    if (Test-Path -LiteralPath $DocxPath) { Remove-Item -LiteralPath $DocxPath -Force }
    $doc.SaveAs2($DocxPath, 16)         # 16 = wdFormatDocumentDefault(.docx)
    Write-Host ("文档   : 页数 {0} / 字符 {1} / 内嵌图片 {2} / 表格 {3}" -f `
            $doc.ComputeStatistics(2), $doc.ComputeStatistics(3), $doc.InlineShapes.Count, $doc.Tables.Count)
    $doc.Close(0); $doc = $null
} finally {
    if ($word) { try { $word.Quit(0) } catch {} ; [void][System.Runtime.InteropServices.Marshal]::ReleaseComObject($word) }
}
Start-Sleep -Milliseconds 400

# ============================ 校验 ============================
$zip = [System.IO.Compression.ZipFile]::OpenRead($DocxPath)
$media = @($zip.Entries | Where-Object { $_.FullName -like 'word/media/*' })
$xmlEntry = $zip.Entries | Where-Object { $_.FullName -eq 'word/document.xml' }
$sr = New-Object System.IO.StreamReader($xmlEntry.Open()); $xml = $sr.ReadToEnd(); $sr.Close()
$zip.Dispose()

$embeds = ([regex]::Matches($xml, 'r:embed=')).Count
Write-Host ""
Write-Host "---- 校验 ----"
Write-Host ("文档大小       : {0:N0} KB" -f ((Get-Item -LiteralPath $DocxPath).Length / 1KB))
Write-Host ("word/media 条目 : {0} 张（预期 {1}）" -f $media.Count, $imgBlocks.Count)
Write-Host ("r:embed 引用   : {0}（预期 {1}）" -f $embeds, $imgBlocks.Count)
Write-Host ("含中文字符     : {0}" -f $(if ($xml -match '马鞍波') { '是 ✓' } else { '否 ✗' }))

if (($media.Count -eq $imgBlocks.Count) -and ($embeds -eq $imgBlocks.Count)) {
    Write-Host "结果           : 通过，图片已内嵌 ✓"
    exit 0
} else {
    Write-Host "结果           : 失败——图片没有内嵌 ✗" -ForegroundColor Red
    exit 1
}
