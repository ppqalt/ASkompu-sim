# ASkompu Core / Ytimen versio — simulaattori 1.1.1

Valitsin löytyy yläpalkin **ASkompu-versio** -painikkeesta. Se keskeyttää
nykyisen simulaatioajon; voit palata Simulaattori-painikkeella ja jatkaa sitä.
Käytössä oleva ASkompu-ydin ja simulaattorin oma versio näytetään erikseen.
Täyden SHA:n voi kopioida napsauttamalla sitä. Paikallisesti muutettu
lähdetyöpuu merkitään näkyvästi, eikä pelkkää HEADia väitetä puhtaaksi lähteeksi.

## Mikky100:n käyttöpolku

1. Avaa **ASkompu-versio** ja tarkista rakennustyökalujen tila.
2. Paina **Päivitä versiolista**. Lähde on `Mikky100/ASkompu` GitHubissa.
3. Valitse **main**, tagi tai commit. Voit suodattaa listaa tai kirjoittaa
   täyden 40-merkkisen SHA:n ja painaa **Ratkaise SHA**. Lista sisältää mainin,
   enintään 100 tagia ja mainin 60 viimeisintä committia. Kaikki versiot eivät
   ole numeroituja julkaisuja. Valinnan tiedoista näkee täyden SHA:n ja viestin.
4. Paina **Rakenna ja testaa**. Valittu SHA pysyy lukittuna, vaikka
   upstream/main muuttuisi sillä välin. Vaiheet ja kulunut aika näkyvät
   näkymän yläosassa. **Tekninen loki** näyttää työkalujen todelliset tulosteet.
5. **Peruuta toiminto** pysäyttää myös rakennuksen lapsiprosessit. Nykyinen
   ohjelma säilyy. Vain yksi backend-operaatio voi käyttää samaa työtilaa.
6. Kun näet **ASkompu-ydin valmis**, valitse **Avaa rakennettu versio** tai
   **Myöhemmin**. Uudelleenkäynnistys pyytää vahvistuksen, koska muistissa olevaa
   ajoa ei siirretä. Uusi sessio alkaa kellon alustuksesta.
7. Uuden version näkymästä pääsee **Palaa edelliseen ohjelmaan** -painikkeella
   takaisin. Käynnistysvirhe jättää nykyisen ikkunan käyttöön.

Versiolistan päivittäminen ei lataa tai rakenna ohjelmaa automaattisesti.
Vain hyväksytty rakennus ja testit avaavat uudelleenkäynnistyksen. Vanhaan
versioon palaaminen aloittaa sekin uuden session. Ohjelma ei päivitä
alkuperäistä Git-työpuuta, yhdistä haaroja tai korvaa käynnissä olevaa EXEä.

## Windows-käyttäjän vaatimukset

Tavallinen simulointi toimii edelleen puretusta paketista ilman
kehitystyökaluja. **Ytimen paikallinen rakentaminen tarvitsee**:

- Python 3.10+ (`py -3` tai `python`), myös PATHissa.
- Git for Windows, CMake 3.25+ ja sen CTest, PATHissa.
- Visual Studio 2022 tai Build Tools 2022: C++-työpöytäkehitys, MSVC x64
  ja Windows SDK. Pelkkä Visual Studio Code ei sisällä kääntäjää.
- Verkkoyhteys lähdekoodin ja SHA-256-lukittujen SDL/ImGui/Unity-lähteiden hakuun.

Valitsin tarkistaa ohjelmat, CMake-version, VS2022 C++-asennuksen `vswhere`:lla
sekä Windows SDK:n otsake- ja x64-kirjastot. Määritys tarkistaa lopullisen
kääntäjä-/SDK-yhdistelmän. Asennusten jälkeen simulaattori kannattaa käynnistää
uudelleen, jotta muuttunut PATH tulee käyttöön. Ominaisuus ei asenna työkaluja
itse eikä ole kehitystyökaluista riippumaton yhden painikkeen päivityspalvelu.

Linuxilla tarvitaan Python 3.10+, Git, CMake/CTest, GCC C++, Make ja SDL:n
näyttöjärjestelmän kehityskirjastot. Preset on `linux-release`; Windowsilla
käytetään oikeaa `windows-release`-presetiä ja Visual Studio 2022 x64:ää.

## Lähteet, versiolukitus ja yhteensopivuus

CMake-valinta `ASKOMPU_CORE_ROOT` osoittaa oikeaan erilliseen tuotantolähdepuuhun.
Oletuksena se on edelleen simulaattoriforkin juuri. Erillinen lähdepuu vaatii
`ASKOMPU_CORE_REVISION`-valinnassa täyden SHA:n, puhtaan Git-checkoutin ja
juurihakemiston täsmällisen vastaavuuden. Tarkistus suoritetaan määrityksessä
ja jokaisessa metadatan rakennuksessa. Väärä SHA tai muutettu puu hylätään.
Paikallisen kansion valinta käyttää jäljempänä kuvattua manifestiin lukittua
tilannekuvaa, joten myös committoimattomat muutokset voidaan rakentaa turvallisesti.

Kaikki core-, domain-, route-, input- ja tuotannon näyttöapulähteet sekä
alkuperäiset viisi host-testisarjaa tulevat valitusta puusta. Simulaattori ei
varmistamattomassa tilanteessa vaihda niitä nykyisen forkin lähteisiin.

`compat/DisplayFormatting.h` on kolmen esitysmuotoiluapurin sovitin: tripin,
aikaeron ja pistevälin tekstimuotoilu. Se perustuu v0.2.5:n samaan otsakkeeseen.
Vanha upstream pitää apurit yksityisinä TFT:n DisplayView.cpp:ssä. Tämä
sovitin erottaa desktop-esityksen eikä sisällä kilpailu-, reitti-, aika- tai
pulssilogiikkaa. Valittua tuotantokoodia ei paikata hiljaisesti. API-muutokset
voivat siten aiheuttaa selvästi ilmoitetun yhteensopivuusvirheen.

`BUILDINFO.txt`, Ohje/Tietoja, versionvalitsin ja `--versio` erottavat
simulaattorin lähderevision ja **todellisen ASkompu-ytimen SHA:n, päivämäärän,
mahdollisen tagin ja puhtaustilan**. ASkompun lähdetiiviste lasketaan valitusta
puusta. Simulaattorin lähdepaketti säilyttää oman revision ja puhtaustilan
SOURCE-ORIGIN.txt:ssä myös silloin, kun sen mukana ei ole .git-hakemistoa.

## Työtila ja restart

GUI käyttää SDL:n käyttäjäkohtaista asetushakemistoa `ASkompu/Simulator` ja
sen `local-core-workflow`-alihakemistoa. Polun voi antaa kehittäjän
`--core-home POLKU` -valitsimella. Työtilassa ovat Git-välimuisti,
operaatiokohtaiset lokit, erilliset rakennukset ja käynnistyshistoria.
Jokainen rakennus saa uuden tunnisteen sekä oman `core`-tilannekuvan ja
`app`-hakemistonsa. GitHub-rakennuksella on myös oma `source`-hakemisto;
paikallinen rakennus käyttää alla kuvattua lähde- ja käännösvälimuistia.
Välimuistin alkuperä ja sisällöt tarkistetaan ennen uudelleenkäyttöä.

Pakettiin sisältyy `simulator-source.zip` sekä `tools/core_versions.py`.
Lähdepaketti sisältää simulaattorin, esityssovittimen, testit ja lisenssin;
valittu tuotantokoodi kloonataan erikseen. Tämä toimii myös paikallisesti
tehdystä, vielä pushaamattomasta simulaattoriversiosta. Lähdepaketti kopioidaan
uuteen rakennukseen, joten käyttäjän työpuuta ei muokata.

Backend rakentaa, ajaa CTestin, asentaa erilliseen `app`-hakemistoon,
tarkistaa uuden ohjelman ilmoittaman SHA:n, paikallisen lähteen tiivisteen
ja täsmälleen pyydetyn näyttöprofiilin sekä ajaa SDL-dummy-GUI-testin.
Vasta sitten se kirjoittaa valmiin rakennuksen tiedot ja EXEn SHA-256:n.
Käynnistyksessä tiiviste tarkistetaan uudelleen. Windows-asennus käyttää
edelleen staattisen runtime-paketin DLL-auditointia.

Asennus ja asennetun ohjelman varmennus ovat omat näkyvät vaiheensa testien
jälkeen. Työkalutarkistus palauttaa aiemman valmiin rakennuksen vain, jos
rakennuskohtainen `ready.json`, sen testimerkintä, SHA/profiili/lähdetiedot,
ohjelman hallittu polku ja binääritiiviste täsmäävät. Poistettu tai muuttunut
rakennus ei jää näkyviin valmiina. Virheellinen rakennus- tai palautushistoria
näytetään varoituksena; se ei estä toimivien rakennustyökalujen käyttämistä.

Backend käynnistää erillisen EXEn. Uusi ohjelma kirjoittaa yksilöllisen
kuittauksen ensimmäisen onnistuneesti piirretyn ruudun jälkeen. Vasta tämän
jälkeen GUI sulkee vanhan ikkunan. Puuttuva kuittaus, prosessin virhe tai
muuttunut EXE estää vaihdon. Peruutus tarkistetaan myös ensimmäisen ruudun
kuittauksen jälkeisen vakausodotuksen jälkeen, ennen käynnistyshistorian
muuttamista. `history.json` säilyttää edellisen ohjelman polun ja SHA-256:n.
Myös takaisin vaihtaminen varmistaa tiivisteen. Vanha palautushistoria ilman
binääritiivistettä tai myöhemmin muuttunut ohjelma hylätään turvallisesti.
CLI-käynnistys raportoi onnistumisen, mutta ei sulje käyttäjän muita ikkunoita.

## Paikallisen ASkompun kehittäminen

Valitse **Paikallinen kansio**, anna ASkompu-projektin juurihakemisto ja paina
**Tarkista kansio**. Kansiossa pitää olla tuotantokoodi (`src` ja `include`)
sekä ASkompun nykyiset host-testit (`test`). Valitse 320×170 tai 480×320 ja
paina **Rakenna ja testaa**. Kun rakennus on hyväksytty, vaihda siihen
**Avaa rakennettu versio** -painikkeella. Tallennetut, committoimattomat ja
versionhallintaan vielä lisäämättömät lähdetiedostot tulevat mukaan.

Git-committia tai pushia ei tarvita tähän kehityskierrokseen. Muokkaa ja
tallenna lähteet, paina uudelleen **Rakenna ja testaa** ja käynnistä valmis
versio. Muistissa olevaa simulaatioajoa ei siirretä uuteen prosessiin.
Kansion tarkistus varmistaa lähderakenteen; todellinen API-yhteensopivuus
varmistuu vasta käännöksessä ja testeissä.

Backend lukee lähteet eikä muuta lähdekansiota, sen Git-indeksiä tai haaraa.
Jokaiselle rakennukselle otetaan täsmällinen tilannekuva hakemistoista `src`,
`include` ja `test` sekä olemassa olevista `LICENSE`, `LICENSE.txt`, `COPYING`
ja `platformio.ini` -tiedostoista. `.git`, `.pio`, `simulator`, `build`,
Python-välimuistit ja tavalliset käännöstuotteet jätetään pois. Symbolisia
linkkejä lähdepuussa ei hyväksytä. Rakennuksen työtilan pitää olla
lähdekansion ulkopuolella.

Lähdetiedostojen sisältö, tiedostoluettelo ja Git-tiedot luetaan ennen ja
jälkeen kopioinnin. Jos ne muuttuvat kopioinnin aikana, toiminto keskeytyy
ja sitä voi yrittää uudelleen tallennuksen jälkeen. Kopioinnin valmistuttua
lähteitä voi kehittää edelleen: käynnissä oleva rakennus käyttää jo lukittua
tilannekuvaa. Tämä on tarkistettu kopio, ei käyttöjärjestelmän atominen
monen tiedoston tilannekuva.

`snapshot-manifest.json` säilyttää alkuperäisen lähdepolun, todellisen Git-HEADin
(jos hakemisto on Git-repon juuri), commitin päiväyksen/tagit, puhtaustilan,
jokaisen tiedoston SHA-256:n ja koko tilannekuvan SHA-256:n. Committoimaton
versio ei saa keksittyä Git-tunnistetta eikä sitä merkitä puhtaaksi commitiksi.
Myös ilman `.git`-hakemistoa oleva lähde voidaan rakentaa; silloin Git-revisio
on `tuntematon` ja sisältö yksilöidään SHA-256:lla.

`ASKOMPU_CORE_SNAPSHOT` on paikallisen tilannekuvan erillinen CMake-polku.
CMake tarkistaa manifestin tiedostoluettelon ja tiivisteet sekä hylkää
polkupakenemiset, linkit, ylimääräiset tiedostot ja muuttuneet sisällöt.
GitHub-rakennus ei käytä tätä polkua: sen puhtaan, täyteen 40-merkkiseen
SHA:han lukitun checkoutin tarkistukset säilyvät ennallaan.

Paikallisen kehityskierroksen nopeuttamiseksi `local-cache` säilyttää erillisen
kopion ja CMake-rakennushakemiston. Välimuistin avain sisältää alkuperäisen
lähdepolun, näyttöprofiilin, simulaattorilähdepaketin täsmällisen sisältötiivisteen,
käyttöjärjestelmän/arkkitehtuurin ja riippuvuuksien mahdolliset lähdepolut.
Saman avaimen rakennus päivittää vain muuttuneet tiedostot ja poistaa
poistetut lähteet. Muuttumattomien tiedostojen aikaleimat säilyvät, jotta
käännös voi olla inkrementaalinen. ZIP-aikaleimat eivät vaihda sisältöavainta.
Kaikki käännös-, testi-, asennus- ja käynnistystarkistukset suoritetaan
jokaisella kierroksella. Lähdevälimuistin muutokset hylätään, eikä tulosta
hyväksytä aiemman testituloksen perusteella.

Lähdepäivitys valmistellaan erillisessä hakemistossa ja tarkistetaan ennen
käytössä olevan välimuistikopion vaihtamista. Muuttumattomien lähteiden
aikaleimat säilyvät myös tässä kopiossa. Vaihto käyttää merkittyä
hakemistotransaktiota ja pitää aiemman varmennetun kopion, kunnes uusi on
hyväksytty. Kirjoitusvirhe tai peruutus palauttaa aiemman kopion. Jos prosessi
katkeaa hakemistovaihdon välissä, seuraava yritys palauttaa vain tunnistetun
transaktion täsmällisiin manifestitiivisteisiin vastaavan kopion. Poikkeavat
sisällöt, muuttuneet palautusmerkinnät ja merkitsemättömän välimuistin
muutokset hylätään; niitä ei korjata hiljaisesti luotetuiksi lähteiksi.

Hyväksytty ohjelma asennetaan aina omaan rakennuskohtaiseen `app`-hakemistoon,
ja alkuperäinen tilannekuva säilyy sen rinnalla. Aiempi toimiva ohjelma ja
rollback-polku säilyvät. `--versio` ja `BUILDINFO.txt` näyttävät paikallisen
polun, lähdetyypin, HEADin, paikallisten muutosten tilan ja tilannekuvan
tiivisteen. Paikallisen tilannekuvan tiiviste kattaa myös testit ja mukaan
otetut lisenssi-/asetustiedostot; Git-rakennuksen vanha lähdetiiviste kattaa
edelleen `src`- ja `include`-puut.

Paikallisen lähteen tarkistus ei käytä verkkoyhteyttä. Ensimmäinen rakennus
voi tarvita verkon lukittujen SDL/ImGui/Unity-riippuvuuksien lataamiseen.
Saman välimuistin seuraavat rakennukset käyttävät jo ladattuja riippuvuuksia.

Vanhoja rakennuksia ei poisteta automaattisesti. Ne ja lokit voivat käyttää
paljon levytilaa. Sulje ohjelmat ennen käsin tehtävää siivousta ja säilytä
käytössä oleva sekä palautukseen tarvittava `app`-hakemisto. Alkuperäinen
ladattu julkaisu on lisäksi riippumaton varaversio.

## Komentorivi ja testaus

Sama backend on `tools/core_versions.py`; GUI:ssa ei ole rinnakkaista Git-
tai CMake-logiikkaa. Se kirjoittaa JSON/TSV-tilan atomisesti ja pitää tekniset
lokit erillään. Pythonin prosessiryhmä (Linux) tai Windows Job Object huolehtii
rakennuksen lapsiprosessien peruutuksesta.

```sh
python3 simulator/tools/core_versions.py doctor --home /oma/erillinen/tyotila --bundle simulator/build/linux-release/simulator-source.zip
python3 simulator/tools/core_versions.py list --home /oma/erillinen/tyotila
python3 simulator/tools/core_versions.py resolve --home /oma/erillinen/tyotila --ref refs/tags/v0.1.0
python3 simulator/tools/core_versions.py build --home /oma/erillinen/tyotila --ref TAYSI_SHA --bundle simulator/build/linux-release/simulator-source.zip
python3 simulator/tools/core_versions.py inspect-local --home /oma/erillinen/tyotila --local /oma/ASkompu
python3 simulator/tools/core_versions.py build --home /oma/erillinen/tyotila --local /oma/ASkompu --profile 480x320 --bundle simulator/build/linux-release/simulator-source.zip
```

Windowsissa käytä `py -3` ja Windows-polkuja; lähdepaketti on
`build/windows-release/simulator-source.zip`. `--job` määrää operaation
lokihakemiston. Luo sinne `cancel`-tiedosto CLI-operaation peruuttamiseksi.
`--local` ja `--ref` ovat toisensa poissulkevat. `--profile` hyväksyy arvot
`320x170` ja `480x320` (oletus), myös GitHub-rakennuksessa.
`launch --artifact TUNNISTE --current NYKYINEN_EXE` ja
`rollback --current NYKYINEN_EXE` käyttävät samaa käynnistyspolkua kuin GUI.

```sh
python3 -m unittest discover -s simulator/tests -p 'test_*.py' -v
./simulator/build/linux-release/askompu-simulaattori --tarkista-ytimenvalitsin --core-home /erillinen/testityotila --core-upstream /paikallinen/git-repo --tarkistuskuvat /olemassa/oleva/kansio
```

Valitsintesti tarkistaa oikeat ImGui-ohjaimet, haun, pienen ikkunan ja
virhetilan. `--testaa-ytimen-rakennus` lisää oikean rakennuksen, peruutuksen,
uusintarakennuksen, testit ja restartin. Käytä erillistä testityötilaa;
onnistuminen jättää uuden simulaattori-ikkunan auki. Tavallinen `--tarkista`
säilyttää nykyisen simulaation GUI-regressiotestin.

Tämän kehitysvaiheen Linux-testien tulokset dokumentoidaan toimitusraportissa.
Uuden versionvalitsimen Windows-prosessien, SDK-tunnistuksen ja restartin
manuaalinen varmennus edellyttää Windows-konetta. Aiemman 0.2.5-julkaisun
Windows-varmennus ei itsessään varmista tätä uutta ominaisuutta.
