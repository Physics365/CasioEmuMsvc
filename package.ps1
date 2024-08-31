mkdir ".\ReleasePackage"

mkdir ".\ReleasePackage\x86"
cp -Recurse .\Release\* .\ReleasePackage\x86

mkdir ".\ReleasePackage\x64"
cp -Recurse .\x64\Release\* .\ReleasePackage\x64

Compress-Archive -Path .\ReleasePackage\x86 -DestinationPath Release-x86.zip -Force
Compress-Archive -Path .\ReleasePackage\x64 -DestinationPath Release-x64.zip -Force

del -Recurse ReleasePackage
