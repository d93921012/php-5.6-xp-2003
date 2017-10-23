# Run PHP 5.6+ on Windows XP and 2003

Instead of patching and rebuilding the whole PHP completely, which is a complicated work, this work implements the API functions that are only available after Vista and then patches the import descriptors of the executable files.

The implemented functions are listed as follows.

<table align="center" border="1">
<tbody>
<tr>
<td>DLL file</td>
<td>function name</td>
<td>ver. of nonxp.dll</td>
</tr>
<tr>
<th>iphlpapi.dll</th>
<td>if_nametoindex</td>
<td>0.0.1.11</td>
</tr>
<tr>
<th rowspan="8">kernel32.dll</th>
<td>GetFinalPathNameByHandleA</td>
<td>0.0.2.20</td>
</tr>
<tr>
<td>GetFinalPathNameByHandleW</td>
<td>0.0.2.20</td>
</tr>
<tr>
<td>GetTickCount64</td>
<td>0.0.1.11</td>
</tr>
<tr>
<td>InitializeConditionVariable</td>
<td>0.0.4.40</td>
</tr>
<tr>
<td>InitializeCriticalSectionEx</td>
<td>0.0.3.30</td>
</tr>
<tr>
<td>SleepConditionVariableCS</td>
<td>0.0.4.40</td>
</tr>
<tr>
<td>WakeAllConditionVariable</td>
<td>0.0.4.40</td>
</tr>
<tr>
<td>WakeConditionVariable</td>
<td>0.0.4.40</td>
</tr>
<tr>
<th>shell32.dll</th>
<td>SHGetKnownFolderPath</td>
<td>0.0.3.30</td>
</tr>
<tr>
<th rowspan="3">ws2_32.dll</th>
<td>WSASendMsg</td>
<td>0.0.1.11</td>
</tr>
<tr>
<td>inet_ntop</td>
<td>0.0.1.11</td>
</tr>
<tr>
<td>inet_pton</td>
<td>0.0.1.11</td>
</tr>
</tbody></table>

### Patch binary files of PHP
Copy `doiatpatch.bat`, `iatpatcher.exe`, and `nonxp.dll` to the home directory of PHP and then execute `doiatpatch.bat`.

### Original page
The patched php files can be download in the original page: <http://www.lindasc.com/php/>.

### Bugs
The CLI of PHP 7 occurs BUG for unknown reasons; however, its CGI is workable with no BUG.
