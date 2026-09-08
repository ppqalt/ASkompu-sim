# Windows-kehitys ja siirrettävä julkaisu

## Varmennustaso

Simulaattorin versio on **1.0.0**. Julkaisun laatuporttiin kuuluvat MSVC x64
-rakennus, C++- ja Python-testit, DLL-auditin hyväksyntä- ja hylkäystestit
sekä puretun ZIP-paketin GUI- ja versionvalitsimen peruspolun tarkistus
Windows Server 2022:ssa SDL:n dummy-ajurilla.
Toteutuneet tulokset kirjataan [1.0.0-julkaisusivulle](https://github.com/ppqalt/ASkompu-sim/releases/tag/v1.0.0).

**Windows 11 -työpöytäajo ja uuden versionvalitsimen koko MSVC-ydinrakennus /
restart / rollback -ketju ovat manuaalisesti varmentamatta.**
Aiemman 0.2.5-version henkilökohtaisella Windows-kannettavalla tehty ajo ei
varmista uuden versionvalitsimen toimintaa.

## Kehittäjän komennot

Asenna Visual Studio 2022:n C++-työpöytäkehitystyökalut ja Windows SDK sekä
CMake 3.25 tai uudempi. Git tarvitaan Git-työnkulkuun, mutta lähdearkiston
rakentaminen onnistuu ilman Git-metatietoja. Ensimmäinen määritys hakee
lukitut SDL-, ImGui- ja Unity-lähdepaketit verkosta. PlatformIO ja Python
eivät kuulu tavallisen Windows-GUI:n rakennusvaatimuksiin.

Aja `simulator`-hakemistossa PowerShellistä:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
cpack --preset windows-release
```

Preset valitsee Visual Studio 17 2022 -generaattorin, x64:n, kaikki testit,
SDL:n lähderakennuksen ja staattisen MSVC-ajonaikaiskirjaston. Tavallinen
GUI aukeaa ilman erillistä konsoli-ikkunaa. Rakennushakemiston ohjelma:

```powershell
.\build\windows-release\Release\askompu-simulaattori.exe
```

Alatason CMake-komennot ovat edelleen käytettävissä repositorion juuressa:

```powershell
cmake -S simulator -B simulator/build-windows -G "Visual Studio 17 2022" -A x64 -DASKOMPU_BUILD_GUI=ON -DASKOMPU_FETCH_UNITY=ON
cmake --build simulator/build-windows --config Release --parallel 4
ctest --test-dir simulator/build-windows -C Release --output-on-failure
cpack --config simulator/build-windows/CPackConfig.cmake -C Release
```

## ZIP ja ajonaikaiset riippuvuudet

Paketti syntyy polkuun
`build/windows-release/packages/ASkompu-simulaattori-windows-x64.zip`:

```text
ASkompu-simulaattori/
├── askompu-simulaattori.exe
├── README.txt
├── BUILDINFO.txt
├── simulator-source.zip
├── tools/core_versions.py
└── LISENSSIT/
    ├── ASkompu-LICENSE.txt
    ├── SDL-LICENSE.txt
    ├── ImGui-LICENSE.txt
    ├── Roboto-LICENSE.txt
    └── THIRD_PARTY.md
```

Ohjelma ei tarvitse lähderepoa, työhakemistoon asennettua aineistoa tai
ajonaikaista Gitiä. Fontti on EXE:n sisällä. Pakettiin ei asenneta testejä,
CMake-välimuistia, objektitiedostoja, kehitys-DLL:iä. Erillinen simulator-source.zip on versionvalitsimen
rakennuslähde; sen käyttö on valinnaista ja vaatii kehitystyökalut.
Vastaanottajalle toimitetaan ZIP ja sen sisällä oleva lyhyt README;
kehittäjän työkaluja ei tarvita julkaistun sovelluksen ajamiseen.

SDL ja Dear ImGui linkitetään staattisesti. Kaikille samaan ohjelmaan
linkitettäville MSVC-kohteille asetetaan CMake-ominaisuus
`MultiThreaded$<$<CONFIG:Debug>:Debug>` eli Release-rakennuksessa `/MT`.
SDL:n oma `SDL_FORCE_STATIC_VCRT` noudattaa samaa valintaa. Sovellus ei
vaihda CRT:n omistamia olioita erillisten DLL:ien kanssa, koska omat
kirjastot, SDL ja ImGui ovat samassa EXE:ssä. Ratkaisu käyttää
[CMake:n normaalia runtime-valintaa](https://cmake.org/cmake/help/latest/variable/CMAKE_MSVC_RUNTIME_LIBRARY.html)
ja [MSVC:n dokumentoitua /MT-mallia](https://learn.microsoft.com/en-us/cpp/build/reference/md-mt-ld-use-run-time-library).

Tavoite on, ettei vastaanottajan tarvitse asentaa VC++ Redistributablea.
Windows 11:n omia järjestelmäkirjastoja käytetään silti. CPackin asennusvaihe
käyttää saman MSVC-asennuksen dumpbiniä riippuvuuksien tarkistamiseen:
puuttuva DLL tai nimettyjen Windows-järjestelmäkirjastojen ulkopuolinen
riippuvuus (myös VC-runtime-DLL) estää paketin hyväksymisen. Tunnettujen
System32-kirjastojen kohdalla rekursio päättyy käyttöjärjestelmärajaan;
niiden omia valinnaisia sisäisiä DLL-riippuvuuksia ei paketoida. Järjestelmässä sattumalta oleva VC-runtime ei siis riitä
läpäisemään tarkistusta. Debug-, 32-bittinen, muu kuin MSVC- tai
järjestelmä-SDL:ää käyttävä rakennus ei kelpaa tähän julkaisupolkuun.

Staattinen runtime päivitetään rakentamalla julkaisu uudelleen päivitetyillä
kehitystyökaluilla. Riippuvuuksien lähdeversiot ja tiivisteet on lukittu;
CI-runnerin MSVC/SDK-versiot voivat muuttua. Tämä on toistettava rakennus- ja
paketointipolku, ei lupaus eri työkaluketjujen tuottamista bittitarkasti
samoista ZIP-tiedostoista.

## Paketin automaattinen ajotarkistus

CI käyttää PowerShell 7:ää ja tätä tarkistusta `simulator`-hakemistossa:

```powershell
./tools/verify_windows_package.ps1 -Archive ./build/windows-release/packages/ASkompu-simulaattori-windows-x64.zip
```

Se purkaa ZIPin uuteen ääkkösiä ja välilyöntejä sisältävään väliaikaispolkuun,
varmistaa täsmällisen tiedostoluettelon ja ajaa `--versio`- ja
`--tarkista`-toiminnot toisesta työhakemistosta. SDL:n dummy-ajuri tekee tästä
työpöydästä riippumattoman tarkistuksen. Ajolla on aikaraja. Se ei todista
Explorer-käynnistystä, näytönohjaimen toimintaa tai Windows 11 -käyttökokemusta.

GitHubin lataama workflow-artifact voi olla ZIP-kääre, jonka sisällä on
varsinainen `ASkompu-simulaattori-windows-x64.zip`. Toimita vastaanottajalle
varsinainen julkaisu-ZIP. Workflow ei julkaise GitHub Releasea.

## Windows 11:n manuaalinen tarkistuslista

Kirjaa testipäivä, Windows-versio, näytön skaalaus ja BUILDINFO.txt:n tiedot.
Tee tarkistus mieluiten koneella, jolla ei ole kehitysympäristöä.

- [ ] ZIP purkautuu siististi yhteen ASkompu-simulaattori-kansioon.
- [ ] EXE käynnistyy Explorerista kaksoisnapsauttamalla ilman konsoli-ikkunaa.
- [ ] Ei puuttuvan DLL:n tai runtime-asennuksen virheilmoitusta.
- [ ] Suomenkieliset merkit, ääkköset ja × näkyvät oikein.
- [ ] Ikkuna skaalautuu ja vierittyy; testaa myös Windowsin 125/150 % skaalaus.
- [ ] Numerosyöttö ja Enter toimivat; nuolet eivät häiritse muokkausta.
- [ ] Jatka/Keskeytä, 1×/10×/100× ja täsmällinen 0,1 s Askel toimivat.
- [ ] Peruutus vaihtaa suunnan ja oikeat trippiarvot muuttuvat.
- [ ] Suuntapainikkeet, PISTE, AT ja pitkät painallukset toimivat.
- [ ] Resetin Peruuta säilyttää ajon ja vahvistus palauttaa alkutilan.
- [ ] Tapahtumaloki ja Ohje → Tietoja simulaattorista toimivat.
- [ ] Ohjelma sulkeutuu puhtaasti myös ikkunan sulkemispainikkeesta.
- [ ] Käynnistys toimii välilyöntejä sisältävästä kansiosta.
- [ ] Käynnistys toimii ääkkösiä sisältävästä kansiosta ja mahdollisuuksien
      mukaan muun kuin ASCII-käyttäjänimen alta.
- [ ] Ohjelma toimii lähderepon ulkopuolella ilman kehitystyökaluja.

Ohjelmaa ei ole allekirjoitettu. SmartScreenin mainevaroitus on mahdollinen;
varoituksen puuttumista ei luvata. Älä neuvo poistamaan suojausta käytöstä.
Lyhyt loppukäyttäjän teksti on erillään: `release/README-windows.txt`.
[Microsoftin SmartScreen-kuvaus](https://learn.microsoft.com/en-us/windows/security/operating-system-security/virus-and-threat-protection/microsoft-defender-smartscreen/)
selittää varoitusten taustalla olevat maine- ja luottamustarkistukset.

## Ytimen versionvalitsin

[Versionvalitsimen ohje](CORE_VERSIONS.md) kuvaa työkalutunnistuksen, erillisen
rakennuksen ja turvallisen restartin. Tavallinen simulointi ei tarvitse
Pythonia tai kääntäjää. Uuden ominaisuuden Windows-varmennus kirjataan
erikseen; aiemman 0.2.5-paketin testit eivät varmista uutta prosessipolkua.
