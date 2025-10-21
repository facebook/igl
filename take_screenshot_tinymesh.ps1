Start-Sleep -Seconds 2
Add-Type -AssemblyName System.Windows.Forms,System.Drawing
$bounds = [System.Windows.Forms.Screen]::PrimaryScreen.Bounds
$bmp = New-Object System.Drawing.Bitmap $bounds.Width, $bounds.Height
$graphics = [System.Drawing.Graphics]::FromImage($bmp)
$graphics.CopyFromScreen($bounds.Location, [System.Drawing.Point]::Empty, $bounds.Size)
$bmp.Save('C:\Users\rudyb\source\repos\igl\igl\tinymesh_d3d12.png')
$graphics.Dispose()
$bmp.Dispose()
Write-Host "Screenshot saved to tinymesh_d3d12.png"
