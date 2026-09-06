# ASkompu-simulaattori – vaihe 1

Simulaattori on deterministinen, käyttöliittymästä riippumaton C++-moottori.
Se simuloi ajoneuvon nopeutta, pyöräpulsseja, peruutussignaalia, painikkeita
ja aikaa **oikean ASkompu-tuotantoytimen ympärillä**. Tässä vaiheessa ei ole
graafista käyttöliittymää eikä erillistä ajosovellusta. Moottorin voi liittää
omaan C++-ohjelmaan; mukana oleva testiohjelma suorittaa tarkistukset.

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

## Rakentaminen Linuxilla

Tarvitaan CMake 3.16 tai uudempi ja C++17-kääntäjä. Moottori ja sen omat testit
eivät vaadi PlatformIO:ta, Unitya, verkkoyhteyttä tai käyttöliittymäkirjastoja.
Suorita komennot ASkompu-repositorion juuresta:

```sh
cmake -S simulator -B simulator/build -DCMAKE_BUILD_TYPE=Release
cmake --build simulator/build --parallel
ctest --test-dir simulator/build --output-on-failure
```

Pelkkä kirjasto ilman testejä: lisää määritykseen `-DBUILD_TESTING=OFF`.
Kirjastokohde on `askompu_simulator`, joka linkittää `askompu_core`-kohteen.

## Windows

Sama lähdekoodi ja CMake-rakenne on tarkoitettu Windowsille. Esimerkiksi
Visual Studion C++-kehittäjäympäristössä:

```powershell
cmake -S simulator -B simulator/build
cmake --build simulator/build --config Release
ctest --test-dir simulator/build -C Release --output-on-failure
```

MSVC:lle asetetaan UTF-8-lähde- ja merkkijonokoodaus. Moottorissa ei ole
POSIX-, Win32- tai kuorirajapintoja. Linux-rakennus on varmennettu GCC:llä
ja Clangilla. Windows-rakennusta ja -ajoa ei ole tässä vaiheessa varmennettu
Windows-koneella; tämä tarkistus tulee tehdä ennen vaiheen 2 julkaisemista.

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
cmake -S simulator -B simulator/build -DCMAKE_BUILD_TYPE=Release -DASKOMPU_UNITY_SOURCE_DIR:PATH=.pio/libdeps/lilygo-main/Unity/src
cmake --build simulator/build --parallel
ctest --test-dir simulator/build --output-on-failure
```

Oman testiohjelman 23 tapauksen lisäksi näin suoritetaan 139 nykyistä
testitapausta viidessä sarjassa. CTest näyttää yhteensä kuusi testiohjelmaa.
Unity-polku on valinnainen ja käyttäjän antama; CMake ei lataa riippuvuuksia.
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

## Rajapinnan käyttö ja tuleva käyttöliittymä

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

Tuleva suomalainen käyttöliittymä lukee oikeaa `DisplayModel`-tietoa ja käyttää
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

- Ei GUI:ta, kiihdytysmallia, sähköistä kohinaa, fyysistä anturimallia tai
  asetusten palautumista virtakatkosta.
- Ajan palvelu on O(millisekunnit + pulssit + komennot). Erittäin pitkät ajot
  ja hyvin tiheät pulssit vaativat vastaavasti enemmän laskentaa.
- 64-bittinen aika ei muuta tuotannon 32-bittisiä pulssiaikaleimoja tai
  nopeuslaskennan nykyisiä ominaisuuksia. Tuotannon mahdolliset rajoitukset
  näkyvät simulaattorissakin; niitä ei paikata rinnakkaisella laskennalla.
- Uusi pulssilaskenta on kokonaislukupohjainen. Tuotannon näyttönopeus käyttää
  edelleen `float`-arvoa; eri kääntäjien bittitason liukulukuvastaavuutta ei
  luvata. Kilpailutulokset tulevat samoista tuotannon laskentafunktioista.
- Ennen vaihetta 2: varmista MSVC/Windows-ajo, lukitse toistotiedoston versio
  ja tapahtumajärjestys sekä päätä erillisen virtakatko- ja tallennusmallin
  sopimus. GUI käyttää moottoria asiakkaana, eikä omista kilpailulogiikkaa.

Kaikki tässä vaiheessa lisätyt simulaattorin käyttäjätekstit, omien testien
tilaviestit ja virheilmoitukset ovat suomeksi. Tuotannon nimistö ja
rakennustyökalujen omat viestit säilyvät alkuperäisinä.
