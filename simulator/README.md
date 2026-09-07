# ASkompu-simulaattori – vaihe 2.5

Natiivi, suomenkielinen työpöytäsimulaattori oikean ASkompu-tuotantoytimen
ympärillä. SDL2 ja Dear ImGui piirtävät käyttöliittymän; vaiheen 1
deterministinen `SimulatorEngine` tuottaa ajan, pulssit ja painikesyötteet.
Moottoria voi edelleen käyttää ja testata ilman graafisia riippuvuuksia.

## Vaiheen 2.5 kehittäjän pikapolku

Tarvitaan **CMake 3.25+** ja C/C++17-kääntäjä. Komennot ajetaan
`simulator`-hakemistossa, ei repositorion juuressa. Linuxilla tarvitaan myös
Make ja SDL:n käyttämän X11- tai Wayland-järjestelmän kehityskirjastot.
Debian/Ubuntu-ympäristössä X11-polun kirjastot voi asentaa esimerkiksi näin:

```sh
sudo apt-get install build-essential cmake libx11-dev libxext-dev libxcursor-dev libxi-dev libxrandr-dev libxss-dev
```

```sh
cd simulator
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
./build/linux-release/askompu-simulaattori
```

| Preset | Sisältö |
| --- | --- |
| `linux-release` | GCC, Release, GUI ja kaikki C++-testit |
| `linux-debug` | GCC, Debug, GUI ja kaikki C++-testit |
| `linux-clang` | Clang, Release, GUI ja kaikki C++-testit |
| `linux-tests` | GCC, Release, kaikki C++-testit ilman GUI-riippuvuuksia |
| `windows-release` | Visual Studio 2022 / MSVC x64, GUI, kaikki C++-testit ja siirrettävä ZIP |

Kaikki presetit toimivat configure-, build- ja test-komennoissa samalla
nimellä. Rakennukset pysyvät erillisissä `build/<preset>`-hakemistoissa.
Paikalliset omat säädöt kuuluvat Gitistä ohitettuun `CMakeUserPresets.json`:iin.
Presetit eivät sisällä konekohtaisia polkuja.

## Windows-kehittäjän polku ja vastaanottajan ZIP

Asenna Visual Studio 2022:n C++-työpöytätyökalut ja Windows SDK sekä
CMake 3.25+. Aja `simulator`-hakemistossa:

```powershell
cmake --preset windows-release
cmake --build --preset windows-release
ctest --preset windows-release
cpack --preset windows-release
```

Paketti on `build/windows-release/packages/ASkompu-simulaattori-windows-x64.zip`.
Vastaanottaja purkaa sen ja kaksoisnapsauttaa `askompu-simulaattori.exe`:ä.
Kehitystyökaluja tai erillistä asennusta ei tarvita julkaistun paketin ajamiseen.
Lyhyt vastaanottajan ohje on [release/README-windows.txt](release/README-windows.txt).
[Windows-kehittäjän ohje ja manuaalinen tarkistuslista](docs/WINDOWS.md)
kuvaavat runtime-ratkaisun, paketin rakenteen ja tarkistukset.

Ensimmäinen simulaattorijulkaisu on **v0.2.5-esikatseluversio**. Julkaisun
hyväksyntä edellyttää Windows/MSVC-rakennuksen, kaikkien testien, DLL-auditin
ja puretun ZIPin automaattisen ajotarkistuksen onnistumista GitHub Actionsissa.
Todelliset tarkistustulokset ja lataukset kirjataan
[oman forkin julkaisusivulle](https://github.com/ppqalt/ASkompu-sim/releases/tag/v0.2.5).
**Windows 11 -työpöytäajo on edelleen manuaalisesti varmentamatta.**
Simulaattori on työpöydän kehitys- ja kokeilutyökalu, ei alkuperäisen
firmwaren tuotantojulkaisu tai turvallisuussertifioitu ajoneuvon navigointilaite.

Windows-julkaisussa SDL ja MSVC-runtime linkitetään staattisesti; fontti on
EXE:n sisällä. CPackin DLL-auditin tulee vahvistaa, että jäljellä ovat vain
Windowsin omat järjestelmäriippuvuudet. Vastaanottajaa ei ohjata asentamaan
VC++ Redistributablea. CI ei julkaise GitHub Releasea automaattisesti.

Linuxin tarkistuspaketin saa komennolla `cpack --preset linux-release`.
Se syntyy `build/linux-release/packages/`-hakemistoon; Linux-paketti käyttää
edelleen käyttöjärjestelmän jaetun C/C++-runtimekirjaston palveluja eikä ole
yleispätevä eri jakeluille tarkoitettu binaarijulkaisu. CI:n ladattava
esikatselupaketti rakennetaan Ubuntu 24.04 x64:llä ja sen purettu ohjelma
tarkistetaan SDL:n dummy-ajurilla erillisessä ääkköspolussa. Paketin
näyttöpolku on X11 (Wayland-istunnossa XWayland), ei natiivi Wayland.
Binaari tarvitsee vähintään GLIBC_2.38- ja GLIBCXX_3.4.32-symbolit sekä
X11:n ajonaikaiset kirjastot; kehitystyökaluja ei tarvita.

## Riippuvuudet ja alatason CMake-komennot

SDL 2.32.10 ja Dear ImGui 1.91.9b säilyvät versio- ja SHA-256-lukittuina.
Presetit hakevat lisäksi Unity 2.6.1:n ja tarkistavat sen tiivisteen, jotta
kaikki aiemmat host-testit saa ilman PlatformIO-asennusta. Ensimmäinen
määritys tarvitsee verkkoyhteyden. [THIRD_PARTY.md](THIRD_PARTY.md) kuvaa
lisenssit. Desktop-riippuvuuksia ei lisätä firmware-rakennukseen.

Alatason komennot toimivat edelleen repositorion juuresta:

```sh
cmake -S simulator -B simulator/build-desktop -DCMAKE_BUILD_TYPE=Release -DASKOMPU_BUILD_GUI=ON -DASKOMPU_FETCH_UNITY=ON
cmake --build simulator/build-desktop --parallel 4
ctest --test-dir simulator/build-desktop --output-on-failure
./simulator/build-desktop/askompu-simulaattori
```

`ASKOMPU_BUILD_GUI` ja `ASKOMPU_FETCH_UNITY` ovat ilman presetiä oletuksena
OFF. Tämä säilyttää offline-moottorirakennuksen. Oma Unity voidaan antaa
`ASKOMPU_UNITY_SOURCE_DIR`:llä. Valmiit lähdehakemistot voi antaa
`FETCHCONTENT_SOURCE_DIR_SDL`, `FETCHCONTENT_SOURCE_DIR_IMGUI` ja
`FETCHCONTENT_SOURCE_DIR_UNITY_SOURCES` -valinnoilla. Lähteiden pitää vastata
lukittuja versioita; oma lähdehakemisto ohittaa arkiston latauksen ja sen
SHA-tarkistuksen.

`ASKOMPU_USE_SYSTEM_SDL=ON` on valinnainen kehittäjäpolku SDL2 CMake CONFIG
-paketille (vähintään 2.0.18). Siirrettävä Windows-julkaisu vaatii haetun
SDL:n; kehitysvaihtoehdot eivät saa hiljaa muuttaa julkaisutiedostoa.

## Versio ja lähteiden tunnistus

Ohje → Tietoja simulaattorista näyttää version, rakennuslajin, lähderevision,
työpuun muutosmerkinnän, yhteisen upstream-pohjan ja lähdetiivisteet.
`--versio` ja paketin `BUILDINFO.txt` sisältävät täydet tunnisteet.
Tiedot syntyvät rakennettaessa, eivät ajonaikaisilla Git-kutsuilla.

Yhteinen upstream-pohja on HEADin ja paikallisen `upstream/main`:n merge-base,
ei verkosta haettu uusin versio. Git-tiedon puuttuessa näytetään `tuntematon`.
Myös Git-metatiedoton lähdearkisto rakentuu. Erilliset SHA-256-tiivisteet
ASkompun `src/include`-lähteistä ja simulaattorin rakennuslähteistä erottavat
paikalliset muutokset. Ne tunnistavat todelliset lähdetavut, joten myös
rivinvaihtojen muutos voi muuttaa tiivistettä.

## Upstream-päivitys ja CI

`origin` on `ppqalt/ASkompu-sim`, `upstream` on `Mikky100/ASkompu`.
Suositeltu työnkulku on `fetch` ja tietoisesti tehty merge erillisessä
päivityshaarassa. [Upstream-ohje](docs/UPSTREAM.md) sisältää tarkat komennot,
ristiriidan perumisen ja laadun tarkistukset. Ohjelma ei ota verkkoyhteyksiä
eikä päivitä itseään.

Puhdasta Git-haaraa voi tarkistaa Linuxilla muuttamatta sitä:

```sh
python3 simulator/tools/check_upstream.py
```

Työkalu kloonaa HEADin väliaikaishakemistoon, hakee sinne upstream/main:n,
yrittää commititonta mergeä ja rakentaa/testaa yhdistetyt lähteet. Lähderepoon
ei kirjoiteta. Epäonnistuva merge tai testi palauttaa virhekoodin.

GitHub Actionsin laatuportti sisältää Linux GCC:n ja Clangin sekä
Windows/MSVC:n GUI-rakennukset ja kaikki C++-testit. GCC-työ ajaa myös SDL:n
dummy-tarkistuksen ja upstream-työkalun eristystestit. Windows-työ muodostaa
ZIPin, tarkistaa paketin ajon erillisestä ääkköspolusta dummy-ajurilla ja
tallentaa hyväksytyn ZIPin workflow-artifactiksi. Molemmat firmware-kohteet
rakennetaan kerran erillisessä Linux-työssä. Upstream-yhteensopivuustyö on
erikseen käynnistettävä `workflow_dispatch`; se ei yhdistä kehityshaaraa,
tee committeja, pushaa, avaa PR:iä tai julkaise Releasea.

## Käyttö

Sovellus käynnistyy keskeytettynä kellon alustukseen ilman ajomääräystä.
Aseta kellonaika YLÖS/ALAS-painikkeilla; OIKEA siirtyy minuuttikenttään ja
hyväksyy ajan. Perusnäkymässä YLÖS/ALAS avaa aidon ASkompu-valikon.
Ajomääräys luodaan ja sitä muokataan siinä tuotannon omilla toiminnoilla.
Erillistä reittitiedostoeditoria tai esiladattua harjoitusreittiä ei ole.

- **ASkompun näyttö:** tumma, skaalautuva laitenäkymä. Tripit, kellonaika,
  nykyinen ja seuraava pisteväli, aikaero, AT, maali sekä valikko- ja
  muokkausnäkymät luetaan oikeasta `DisplayModel`-oliosta.
- **Ajoneuvo:** liukusäädin 0–200 km/h, Enterillä hyväksyttävä numerosyöttö
  ja pikavalinnat 0, 36, 60 ja 120 km/h. Numerosyötössä voi käyttää pistettä
  desimaalierottimena. Tavoitenopeus näkyy suurena. Pysäytä asettaa nopeudeksi
  nollan; Peruutus vaihtaa tuotannolle toimitettua peruutussignaalia.
- **ASkompun painikkeet:** VASEN, YLÖS, ALAS, OIKEA, PISTE ja AT.
  Avattavassa lisäosassa ovat pitkä VASEN, pitkä PISTE ja kolme
  nollauspainiketta. Pitkä painallus on erillinen tietoinen toiminto.
  Ulkoinen nollaus ja jalkanollaus noudattavat ytimen yhteistä kohdeasetusta.
- **Jatka / Keskeytä:** käynnistää tai pysäyttää simuloidun ajan etenemisen.
  Keskeytys säilyttää ajoneuvon tavoitenopeuden. Painikkeita ja asetuksia
  voi käyttää myös ajan ollessa keskeytettynä.
- **Askel:** keskeyttää jatkuvan ajon ja etenee täsmälleen **0,1 sekuntia**.
  Kerroin ei muuta askelta. Tämä GUI-toiminto eroaa moottorin 1 ms:n
  `step()`-apumetodista.
- **Simulointinopeus 1× / 10× / 100×:** kerroin määrää pyydetyn simuloidun
  ajan määrän. Se ei muuta kilpailusääntöjä tai pulssilaskentaa.
- **Aloita alusta:** merkityksellinen ajo vaatii vahvistuksen. Valintaikkuna
  keskeyttää ajan; Peruuta palauttaa aiemman käyntitilan. Vahvistus kutsuu
  moottorin `reset()`-metodia ja palauttaa kellon alustuksen, nollanopeuden,
  eteenpäin ajon, keskeytyksen ja kertoimen 1×. Myös muistissa tehdyt
  ajomääräys- ja asetusmuutokset poistuvat.
- **Sisäinen tila:** avattava paneeli näyttää oikean näyttötilan,
  pulssimäärän, kilpailutilan, reittitiedot, pisteet ja kellon käyntiajan.
  Tripit ja kalibrointi ovat aina näkyvissä laitenäytön alla.
- **Tapahtumaloki:** viimeisimmät 512 havaintoa, erilliset Simulaattori- ja
  ASkompu-lähteet, uusimman seuranta ja ASkompu-suodatin. ASkompu-tapahtumat
  luetaan tuotannon tapahtumavarastosta; perumiset näkyvät myös aiemmassa
  kirjauksessa. Havaittu aika tarkoittaa keräyshetken simuloitua aikaa.
  Tapahtuman alkuperäisen ASkompu-kellonajan näkee osoittimen vihjeestä.

Ohje avaa saman keskeisen käyttöopastuksen sovelluksessa. Nuolet ohjaavat
suuntapainikkeita, P = PISTE, A = AT, R = peruutus, S = pysäytä,
N = askel, välilyönti = keskeytä/jatka ja Esc = pitkä VASEN.
Pikanäppäimet eivät toimi tekstisyötön, aktiivisen säätimen tai valintaikkunan
aikana. Tab ottaa käyttöön säätimien näppäimistökohdistuksen ja varaa nuolet
siihen. Hiiren napsautus palauttaa ASkompun pikanäppäimet.

Ikkunan oletuskoko on 1380 × 960 ennen DPI-skaalausta ja vähimmäiskoko
740 × 580. Kapeassa ikkunassa paneelit siirtyvät allekkain ja sisältöä
vieritetään pystysuunnassa. LCD säilyttää suhteensa 480:320. Käynnistyksen
DPI-tieto skaalaa fontteja ja välejä; fontteja ei rakenneta uudelleen
siirryttäessä lennossa eri DPI:n näytölle. Kaikki sovellukseen lisätyt
käyttäjätekstit ovat suomeksi. Tuotannon omat valikkonimet ja tekniset
lyhenteet säilyvät, samoin riippuvuuksien lisenssit ja rakennustyökalujen
alkuperäiskieliset tulosteet.

## GUI-ohjain ja ajan tahdistus

`AppController` omistaa moottorin ja välittää kaikki muutokset sen
julkisiin metodeihin. Näkymälle palautetaan vain `const SimulatorEngine&`.
Näkymä ei kirjoita tuotantotilaan. Kilpailu-, reitti-, JAT-, MITTIS-,
pisteytys- ja kalibrointilaskenta pysyy ennallaan tuotantoytimessä.

Pääsilmukan `steady_clock` mittaa vain todellista ruutuväliä. Ohjain pyytää
sen ja kertoimen perusteella kokonaislukuisia mikrosekunteja moottorilta ja
säilyttää nanosekuntien jakojäännöksen. Tauon aika hylätään. Yli 250 ms:n
ruutuvälistä hyväksytään enintään 250 ms ennen kertomista; ylimenevää osaa
**ei ajeta myöhemmin kiinni**. Rajaus kirjataan varoituksena ja ohitettu
määrä näkyy sisäisessä tilassa. Hidas kone voi siis jäädä valitusta
reaaliaikaisesta nopeudesta, mutta tuotantoytimen laskenta käyttää aina
ainoastaan pyydettyä simuloitua aikaa. Piirtämisen joutokäyntiodotus ei
toteuta simulaation oikeellisuutta.

Laitenäytön `DisplayLayout`- ja `RouteOrderFormatting`-aputoimintoja käytetään
suoraan. Kolme aiemmin `DisplayView.cpp`:ssä ollutta lukumuotoilufunktiota
siirrettiin muuttumattomin rungoin yhteiseen `src/ui/DisplayFormatting.h`-
otsakkeeseen. Vaiheessa 2.5 otsakkeessa korjattiin PRIu64-argumenttien tyypit;
esitystapa säilyi ennallaan. GUI toteuttaa
oman piirron; se ei tavoittele TFT-pikseliemulaatiota.

## Graafinen käynnistystarkistus

```sh
./simulator/build-desktop/askompu-simulaattori --tarkista
```

Tarkistus avaa oikean ikkunan, syöttää ImGui-tason hiiri- ja näppäintapahtumia
näkyviin ohjaimiin, tarkistaa moottorin oikean tilan ja sulkee sovelluksen.
Se käy läpi kellon hyväksymisen, numerosyötön, ajon, peruutuksen, askelluksen,
kertoimet, tauon, ajomääräyksen luonnin, AT:n/perumisen, maalin, resetin ja
koon muuttamisen sekä pienen ikkunan vierityksen ja nopeussyötteen.
Se ei ole kuvapikseleihin perustuva testi eikä kaikkien
käyttöjärjestelmän syötepolkujen kattava testi. Ajo vaatii toimivan
Linux-/Windows-työpöytäistunnon, joten se ei kuulu tavalliseen CTest-ajoon.

Valinnainen `--tarkistuskuvat HAKEMISTO` tallentaa kolme BMP-tarkistuskuvaa
olemassa olevaan hakemistoon. `--ohje` näyttää komentoriviohjeen.

## Arkkitehtuuri

```text
Nopeus → WheelPulseGenerator → ApplicationCore::handleDistancePulses
Peruutus                    → ApplicationCore::handleReverseSignal
Painikkeet                  → ApplicationCore::handleButton
SimTimeSource → SoftwareClock → ApplicationCore::tick
                                      ↓
                      Tuotannon kilpailu-, reitti- ja tapahtumalogiikka
                                      ↓
                                Oikea DisplayModel
```

CMake kääntää suoraan tiedostot `../src/core`, `../src/domain` ja
`../src/route`. Kilpailu-, matka-, pisteytys-, JAT-, MITTIS-, kalibrointi- ja
valikkolaskentaa ei toteuteta uudelleen. Nykyinen `ApplicationCore` ylläpitää
omia trippiarvojaan käyttäen tuotannon `MotionMath`-aputoimintoja; simulaattori
ei kirjoita niitä eikä rakenna rinnakkaista `TripCounter`-instanssia.
Tuotannon julkista rajapintaa ei ole tarvinnut muuttaa.

`SimulatorEngine` omistaa ajan, `SoftwareClock`-kellon, `ApplicationCore`-ytimen,
pulssigeneraattorin ja tulevien komentojen jonon. Moottoria käytetään yhdestä
säikeestä. Kopiointi on estetty kelloviittausten omistuksen säilyttämiseksi.
Arduino-, GPIO-, TFT- ja Preferences-sovittimia ei käännetä simulaattoriin.

## Simuloitu aika ja tapahtumajärjestys

Aika on 64-bittinen kokonaisluku mikrosekunteina. Se alkaa nollasta ja etenee
vain `advanceMicroseconds`, `advanceMilliseconds` tai `step` -kutsusta.
`step()` etenee yhden millisekunnin. `advanceMicroseconds(0)` ei tee mitään.
Seinäkelloa, odottamista, kuvataajuutta tai suorittimen nopeutta ei käytetä.
Minuutin simulointi ei vaadi minuutin odotusta.

`SimTimeSource` toteuttaa tuotannon 32-bittisen millisekuntirajapinnan.
64-bittinen aika on saatavilla erikseen, ja tuotannon mikrosekuntisyötteet
muunnetaan 32-bittisiksi kuten laitteessa. `SoftwareClock` käsittelee
millisekuntilaskurin kierron, kun sitä palvellaan säännöllisesti. Moottori
tekee tämän automaattisesti; erikseen käytettyä `SimTimeSource`-oliota ei saa
hyppäyttää täyttä 32-bittistä millisekuntikierrosta ilman kellon lukemista.
Ajan ylivuoto hylätään poikkeuksella ennen simulaatiotilan muuttamista.

Ydin saa `tick`-kutsun absoluuttisessa 1 ms:n ruudukossa sekä pulssin ja
suoran tai ajastetun komennon jälkeen. Aika-askeleen mielivaltainen loppukohta
ei lisää ylimääräistä `tick`-kutsua. Siksi esimerkiksi AT:n pysähtymisajastin
toimii samalla tavalla riippumatta siitä, pilkotaanko eteneminen 1 µs:n vai
sekuntien kutsuihin. Ajastimen muutos voi näkyä vasta seuraavalla ruudukon
tickillä, alle millisekunnin kuluttua määräajasta, ellei muu tapahtuma palvele
ydintä aiemmin. Näyttömallin lukeminen ei lisää tick-kutsua.

Samalla aikaleimalla tapahtumat suoritetaan seuraavasti:

1. Päättyneen aikavälin pyöräpulssi toimitetaan aiemmalla peruutustilalla ja
   kalibroinnilla.
2. Pulssin tai millisekuntiruudukon `tick` ja tallennuskuittaukset käsitellään.
3. Aikaleiman ajastetut komennot suoritetaan lisäysjärjestyksessä. Jokaisen
   jälkeen suoritetaan `tick`, tallennuskuittaukset ja generaattorin
   kalibroinnin päivitys.

Lyhyt painallus on yksi komento: tuotannon `Press` ja `Release` toimitetaan
peräkkäin samalla aikaleimalla ennen komennon jälkeistä tick-kutsua.
Suorat komennot suoritetaan nykyhetkessä kutsujan määräämässä järjestyksessä.
Ajastaa saa vain tulevaisuuteen; nykyhetkeen käytetään `execute`-metodia.
Komento tarkistetaan jo jonoon lisättäessä. Tämä rajaus estää jo käsiteltyjen
pulssien eteen jälkikäteen lisättävät tapahtumat.

Järjestys on moottorin toistosopimus. Se ei jäljittele ESP32:n ISR-ajoituksen,
10 ms:n laitesilmukan ja pulssierien kaikkia mahdollisia yhteensattumia.
Ydin saa kuitenkin aidot tuotantosyötteet täsmällisesti määritellyssä
järjestyksessä. Samaa syötejonoa verrataan testeissä myös suoraan tuotantoytimeen.

## Pulssisimulaatio ja kalibrointi

Nopeus tallennetaan yksikössä **0,001 km/h**, välillä 0–200 km/h.
`setSpeedMilliKmh(36000)` tarkoittaa 36 km/h. `setSpeedKmh(36.0)` on
käyttömukavuusmetodi, joka pyöristää lähimpään tuhannesosaan; puoliväli
pyöristyy ylöspäin. Toistotiedostojen kannattaa tallentaa kokonaislukuarvo.
Negatiivinen, liian suuri tai ei-äärellinen nopeus hylätään. Peruutus asetetaan
erikseen. Nopeus vaihtuu heti, ilman kiihtyvyysmallia.

Kun nopeus on `v` tuhannesosakilometriä tunnissa ja aika `t` mikrosekuntia:

```text
liike millimetreinä = v × t / 3 600 000
pulssikynnys       = aktiivinen mm/pulssi × 3 600 000
```

Liikkeen osoittaja kertyy kokonaislukuna. Seuraavan pulssin aika lasketaan
jäljellä olevasta osuudesta ylöspäin pyöristettynä mikrosekunteina. Ylijäämä
säilytetään seuraavalle pulssille. Kiinteällä nopeudella ja kalibroinnilla
pulssiaikaleiman virhe on alle 1 µs eikä kasva matkan mukana. Näin
1000 mm/pulssi tuottaa 36 km/h:ssa 100000 µs:n ja 60 km/h:ssa 60000 µs:n
pulssivälin ilman erityistapauksia.

Pysähtyminen ja nopeuden tai suunnan muutos säilyttävät keskeneräisen
pulssivaiheen. Suunnanvaihto muuttaa tuotannolle toimitettua peruutussignaalia;
matkan etumerkin päättää tuotantoydin. Malli ei käsittele anturin mekaanista
kulma-asentoa tai suunnanvaihdon välystä.

Kalibrointi luetaan aina `ApplicationCore::millimetersPerPulse()`-arvosta.
Muutos tehdään aidosta KERROIN-valikosta tai hyväksymällä MITTIS-ehdotus.
Alkukerroin annetaan `InitialState`-rakenteessa. Muuttunut kerroin vaikuttaa
tuleviin pulsseihin ja niiden väliin; vanhoja trippiarvoja ei lasketa uudelleen.
Kalibroinnin vaihtuessa säilytetään keskeneräisen pyöränkierroksen suhteellinen
osuus. Skaalaus pyöristää alaspäin alle yhden liikeosoittajan yksikön eli alle
1/3 600 000 mm. Jaettu kertolasku estää ylivuodon ilman alustakohtaista
128-bittistä lukutyyppiä. Kahden kalibroinnin välissä liike kertyy täsmällisesti.

Pulssit toimitetaan yksi kerrallaan, eikä simulaattori kirjoita kilpailun tai
trippien matkaa suoraan. 200 km/h:n yläraja vastaa nykyisen laitesyötteen
nopeusrajaa. Sähköistä kohinaa, pulssisuodatusta ja kytkinvärähtelyä ei
simuloida: syötetään hyväksyttyjä pulsseja ja semanttisia painiketapahtumia.

## Alkutila, tallennus ja Aloita alusta

Oletusalkutilassa aika on 0 µs, kello asettamatta, näytössä kellon alustus,
nopeus 0, peruutus pois, pulssit ja matkat nollassa, kilpailu IDLE ja
tapahtumaloki tyhjä. Reittiä ei ole. Kalibrointi ja asetukset käyttävät
tuotannon oletuksia. Molemmat ulkoiset nollauspainikkeet (`Trip2Reset` ja
`FootReset`) noudattavat tuotannon `externalResetTarget`-asetusta, jonka
oletus on Trip 1; painikkeen tekninen nimi ei ohita asetusta.

`InitialState` voi sisältää kalibroinnin, ajomääräyksen aktiivisuusmerkintöineen,
kilpailu- ja näyttöasetukset sekä tallennuksen onnistumisasetuksen.
Kelpaamaton kalibrointi tai annettu reitti hylätään. Aktiivinen alkureitti
aktivoituu tuotannon kautta vasta kellon hyväksymisen jälkeen; passiivinen
reitti pysyy selattavana ilman kilpailun aloittamista.

Moottori kuittaa ytimen kalibrointi-, reitti-, reitin valmistumis-, väri-,
debug- ja näyttöasetusten tallennuspyynnöt. Oletuksena kuittaus onnistuu,
ja hyväksytty tila elää ytimen muistissa. `setSavesSucceed(false)` simuloi
tallennusvirhettä. Tämä ei kirjoita levylle eikä toteuta pysyvää NVS-varastoa.
Kuittaukset käyttävät samaa järjestystä kuin `src/main.cpp`.

`reset()` luo ajan, kellon ja ytimen kokonaan uudelleen alkuperäisestä
`InitialState`-kopiosta. Se tyhjentää komentojonon, lokit, tulokset, pidetyt
painikkeet, pulssivaiheen, nopeuden ja peruutustilan. Myös myöhemmin muutetut
asetukset ja tallennusvirheasetus palautuvat alkuarvoihinsa. Alkureitti säilyy
vain alkuperäisen konfiguraation mukaisena. Tämä on **Aloita alusta**, ei
virtakatkon jälkeinen palautuminen. Ytimestä saadut viittaukset ja osoittimet
vanhenevat resetissä; tapahtuma- ja reittiosoittimet voivat vanhentua myös
niitä muuttavan syötteen yhteydessä.

## Moottorin rakentaminen ilman GUI:ta Linuxilla

Tarvitaan CMake 3.25 tai uudempi ja C++17-kääntäjä. Moottori ja sen omat testit
eivät vaadi PlatformIO:ta, Unitya, verkkoyhteyttä tai käyttöliittymäkirjastoja.
Suorita komennot ASkompu-repositorion juuresta:

```sh
cmake -S simulator -B simulator/build -DCMAKE_BUILD_TYPE=Release -DASKOMPU_BUILD_GUI=OFF
cmake --build simulator/build --parallel
ctest --test-dir simulator/build --output-on-failure
```

Pelkkä kirjasto ilman testejä: lisää määritykseen `-DBUILD_TESTING=OFF`.
Kirjastokohde on `askompu_simulator`, joka linkittää `askompu_core`-kohteen.

## Moottorin rakentaminen ilman GUI:ta Windowsilla

Sama lähdekoodi ja CMake-rakenne on tarkoitettu Windowsille. Esimerkiksi
Visual Studion C++-kehittäjäympäristössä:

```powershell
cmake -S simulator -B simulator/build -DASKOMPU_BUILD_GUI=OFF
cmake --build simulator/build --config Release
ctest --test-dir simulator/build -C Release --output-on-failure
```

MSVC:lle asetetaan UTF-8-lähde- ja merkkijonokoodaus. Moottorissa ei ole
POSIX-, Win32- tai kuorirajapintoja. CI tarkistaa Linuxin GCC- ja Clang-
rakennukset sekä Windowsin MSVC-rakennuksen. Julkaisun toteutuneet
tarkistukset kerrotaan julkaisusivulla; Windows 11:n manuaalinen työpöytäajo
on vielä tekemättä.

## Testit ja nykyiset regressiotestit

Oma testiohjelma sisältää 23 testitapausta: aika ja laskurien kierrot,
SoftwareClock, pysähdys, tunnetut pulssivälit ja eri kalibroinnit,
nopeusmuutokset, tunnin murto-osanopeusajo, peruutus, painikkeet, pidetyt
nollaukset, AT, näyttömalli, tallennusvirheet, kalibrointivaihe, MITTIS,
JAT ja pisteytys, toisto, eri kokoiset aika-askeleet, samanaikaisuus,
reset, virhesyötteet, ylivuodot, käsin miinustus, pisteen peruminen,
suora tuotantoydinvertailu sekä reitin ja näyttöasetusten tallennus.
Toistovertailu tarkistaa näyttömallin kentät, kilpailutilan, trippiarvot,
reitin, tulokset ja tapahtumat sisältöineen; muistiosoitteita ei verrata.

Nykyiset viisi Unity-testisarjaa voidaan kääntää **muuttamattomista**
`test/`-lähteistä myös CMakeen. Anna Unity 2.6.1:n `src`-hakemisto.
Esimerkiksi jo asennetulla PlatformIO-riippuvuudella:

```sh
cmake -S simulator -B simulator/build -DCMAKE_BUILD_TYPE=Release -DASKOMPU_BUILD_GUI=OFF -DASKOMPU_UNITY_SOURCE_DIR:PATH="$PWD/.pio/libdeps/lilygo-main/Unity/src"
cmake --build simulator/build --parallel
ctest --test-dir simulator/build --output-on-failure
```

Oman testiohjelman 23 tapauksen lisäksi näin suoritetaan 139 nykyistä
testitapausta viidessä sarjassa. Lisäksi mukana on 13 GUI:sta riippumatonta
ohjaintestiä: tauko/jatkaminen, kertoimet, ruutuajan pilkkominen, jakojäännös,
viiverajaus, askel, ajaminen, painikkeet, reset, loki, yhteinen muotoilu,
virhesyötteet ja tuotantotilan lukuraja. Yhteensä **175 tapausta seitsemässä CTest-ohjelmassa**.
Ilman Unity-polkua suoritetaan 36 tapausta kahdessa ohjelmassa.
Unity-polku on valinnainen ja käyttäjän antama. Vaihtoehtona
`ASKOMPU_FETCH_UNITY=ON` hakee lukitun Unityn automaattisesti. Kun sekä
GUI että Unity-haku ovat OFF, CMake ei lataa riippuvuuksia.
Jos riippuvuus on muualla, anna sen `src`-hakemisto. Vanhojen testien omat
englanninkieliset nimet ja Unityn tulosteet ovat ennallaan.

Nykyinen `pio test -e native` käyttää MinGW-pakettia, Windowsin `cmd.exe`:ä
ja `scripts/run_native_tests.cmd`-tiedostoa. Se ei ole Linuxin toimiva
lähtötaso. Tätä ympäristöä tai `scripts/native_toolchain.py`-tiedostoa ei
ole muutettu, eikä rajoitus ole simulaattorin regressio.

Laiteohjelmiston regressiorakennukset tehdään edelleen näin:

```sh
pio run -e lilygo-main
pio run -e esp32s3-ili9488-main
```

## Moottorirajapinnan käyttö

```cpp
#include "engine/SimulatorEngine.h"

simulator::SimulatorEngine engine;
engine.press(core::ButtonId::Right);  // Tunneista minuutteihin.
engine.press(core::ButtonId::Right);  // Hyväksy 0:00.
engine.setSpeedKmh(36);
engine.schedule(1000000, simulator::Command::reverseState(true));
engine.schedule(1500000, simulator::Command::speed(0));
engine.advanceMilliseconds(2000);

const auto display = engine.displayModel();
const auto trip = engine.application().trip1DistanceMillimeters();
const auto& competition = engine.application().competition();
const auto* route = engine.application().currentRouteOrder();
const auto& events = engine.application().eventRepository();
const auto calibration = engine.application().millimetersPerPulse();
```

`application()` ja `clock()` palauttavat vain lukuun tarkoitetut viittaukset.
Pitkän painalluksen tai pidetyn nollauksen voi syöttää `buttonEvent`-metodilla
käyttäen tuotannon `Press`, `Release`, `LongStart` ja `LongRepeat`-tapahtumia.
Moottori ei lisää automaattista pitkän painalluksen tulkintaa.

Työpöytäkäyttöliittymä lukee oikeaa `DisplayModel`-tietoa ja käyttää
näitä komentoja. Tuotannon näyttömallissa on myös lainattuja tekstiosoittimia:
pidempään säilytettävään käyttöliittymä- tai toistotallenteeseen niiden sisältö
pitää kopioida. Käyttöliittymä voi määrätä, kuinka paljon aikaa edistetään
kerralla. Säätimet 1×, 10× ja 100× eivät muuta moottorin laskentaa.
Tauko tarkoittaa, ettei aikaa edistetä; `stop()` pysäyttää vain ajoneuvon.

Komentojono on pieni pohja tulevalle toistolle. Tiedostomuotoa, tuontia,
vientiä, satunnaistusta tai skenaariokehystä ei vielä ole. Toiston tulee
tallentaa alkukonfiguraatio, kokonaislukunopeudet, aikaleimat ja saman ajan
komentojen järjestys. Tallennusvirheiden ohjaus on tarvittaessa tallennettava
myös; reset aloittaa uuden aikajanan.

## Rajoitukset ja seuraava vaihe

- Ei kiihdytysmallia, sähköistä kohinaa, fyysistä anturimallia tai
  asetusten palautumista virtakatkosta.
- Ajan palvelu on O(millisekunnit + pulssit + komennot). Erittäin pitkät ajot
  ja hyvin tiheät pulssit vaativat vastaavasti enemmän laskentaa.
- 64-bittinen aika ei muuta tuotannon 32-bittisiä pulssiaikaleimoja tai
  nopeuslaskennan nykyisiä ominaisuuksia. Tuotannon mahdolliset rajoitukset
  näkyvät simulaattorissakin; niitä ei paikata rinnakkaisella laskennalla.
- Uusi pulssilaskenta on kokonaislukupohjainen. Tuotannon näyttönopeus käyttää
  edelleen `float`-arvoa; eri kääntäjien bittitason liukulukuvastaavuutta ei
  luvata. Kilpailutulokset tulevat samoista tuotannon laskentafunktioista.
- GUI:n loki on rajattu, mutta tuotannon muistissa oleva tapahtumavarasto
  säilyttää ajon tapahtumat edelleen rajoittamatta. Pitkän ajon voi aloittaa
  alusta; varaston säilytyssopimus kuuluu myöhempään vaiheeseen.
- Ei tiedostojen tuontia/vientiä, tallennettua toistoa, verkkoa, Bluetoothia
  tai laitekytkentää. Ajomääräys ja asetukset ovat vain ajon muistissa.
- Windows 11:n manuaalinen työpöytäajo, usean näytön vaihtuva DPI ja
  ruudunlukijan käyttö on vielä varmennettava erikseen.
- Vaiheessa 3 kannattaa määrittää versioitu skenaario-/toistotiedosto ja
  tallennusmalli sekä hyödyntää moottorin valmista komentojonoa. GUI pysyy
  moottorin asiakkaana eikä omista kilpailulogiikkaa.

Kaikki tässä vaiheessa lisätyt simulaattorin käyttäjätekstit, omien testien
tilaviestit ja virheilmoitukset ovat suomeksi. Tuotannon nimistö ja
rakennustyökalujen omat viestit säilyvät alkuperäisinä.
