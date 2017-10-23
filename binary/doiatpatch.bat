@for /f %%i in ('dir /b /s *.exe') do iatpatcher "%%i"
@for /f %%i in ('dir /b /s *.dll') do iatpatcher "%%i"

