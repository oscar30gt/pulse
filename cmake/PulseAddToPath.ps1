# Adds a directory to the user PATH (HKCU\Environment, no administrator rights needed), unless it is already
# there. Run by cmake/PulseAddToPath.cmake during `cmake --install` with -DPULSE_ADD_TO_PATH=ON.
# The registry is edited directly instead of through [Environment]::SetEnvironmentVariable, which would
# expand the %VARIABLES% of the existing entries for good.
param([Parameter(Mandatory = $true)][string]$Directory)

$ErrorActionPreference = 'Stop'
$key = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey('Environment', $true)
try {
    $path = [string]$key.GetValue('Path', '', [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
    $kind = [Microsoft.Win32.RegistryValueKind]::ExpandString
    if ($path) {
        $kind = $key.GetValueKind('Path')
    }

    $entries = @($path -split ';' | Where-Object { $_ })
    $wanted = $Directory.TrimEnd('\')
    foreach ($entry in $entries) {
        if ([Environment]::ExpandEnvironmentVariables($entry).TrimEnd('\') -ieq $wanted) {
            Write-Output "$Directory is already on the user PATH"
            exit 0
        }
    }

    $key.SetValue('Path', (($entries + $Directory) -join ';'), $kind)
}
finally {
    $key.Close()
}

# Tell running programs (Explorer, new terminals) that the environment changed
Add-Type -Namespace PulseInstall -Name NativeMethods -MemberDefinition @'
[DllImport("user32.dll", SetLastError = true, CharSet = CharSet.Unicode)]
public static extern IntPtr SendMessageTimeout(IntPtr hWnd, uint Msg, UIntPtr wParam, string lParam,
    uint fuFlags, uint uTimeout, out UIntPtr lpdwResult);
'@
$result = [UIntPtr]::Zero
[void][PulseInstall.NativeMethods]::SendMessageTimeout([IntPtr]0xffff, 0x1A, [UIntPtr]::Zero, 'Environment', 2, 5000, [ref]$result)

Write-Output "Added $Directory to the user PATH; open a new terminal to use pulse"
