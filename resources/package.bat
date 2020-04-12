del stunserver_win64_1_2_16.zip
set STEXE="C:\Program Files (x86)\Windows Kits\10\bin\10.0.18362.0\x64\signtool.exe"

rmdir /s release
mkdir release
copy ..\stunserver.exe release\
copy ..\stunclient.exe release\
copy ..\stuntestcode.exe release\
copy ..\HISTORY release\HISTORY.txt
copy ..\LICENSE release\LICENSE.txt
copy ..\README release\README.txt
copy ..\testcode\stun.conf release\


copy d:\cygwin64\bin\cygcrypto-1.1.dll release\
copy d:\cygwin64\bin\cygcrypto-1.1.dll release\
copy d:\cygwin64\bin\cygwin1.dll release\
copy d:\cygwin64\bin\cygz.dll release\
copy d:\cygwin64\bin\cyggcc_s-seh-1.dll release\
copy "d:\cygwin64\bin\cygstdc++-6.dll" release\

echo "prepare for code signing"
pause

cd release
%STEXE% sign /a     /fd sha1 /td sha1      /tr http://timestamp.comodoca.com/rfc3161 /v *.exe
%STEXE% sign /a     /fd sha1 /td sha1      /tr http://timestamp.comodoca.com/rfc3161 /v *.dll
%STEXE% sign /a /as /fd sha256 /td sha256  /tr http://timestamp.comodoca.com/rfc3161 /v *.exe
%STEXE% sign /a /as /fd sha256 /td sha256  /tr http://timestamp.comodoca.com/rfc3161 /v *.dll
cd ..

c:\7zip\7z.exe a stunserver_win64_1_2_16.zip release\*.*

