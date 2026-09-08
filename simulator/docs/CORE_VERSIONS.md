# ASkompu Core / Ytimen versio — simulaattori 1.0.0

Valitsin löytyy yläpalkin **Ytimen versio** -painikkeesta. Se keskeyttää
nykyisen simulaatioajon; voit palata Simulaattori-painikkeella ja jatkaa sitä.
Käytössä oleva ASkompu-ydin ja simulaattorin oma versio näytetään erikseen.
Täyden SHA:n voi kopioida napsauttamalla sitä. Paikallisesti muutettu
lähdetyöpuu merkitään näkyvästi, eikä pelkkää HEADia väitetä puhtaaksi lähteeksi.

## Mikky100:n käyttöpolku

1. Avaa **Ytimen versio** ja tarkista rakennustyökalujen tila.
2. Paina **Päivitä versiolista**. Lähde on `Mikky100/ASkompu` GitHubissa.
3. Valitse **main**, tagi tai commit. Voit suodattaa listaa tai kirjoittaa
   täyden 40-merkkisen SHA:n ja painaa **Ratkaise SHA**. Lista sisältää mainin,
   enintään 100 tagia ja mainin 60 viimeisintä committia. Kaikki versiot eivät
   ole numeroituja julkaisuja. Valinnan tiedoista näkee täyden SHA:n ja viestin.
4. Paina **Rakenna valittu ydin**. Valittu SHA pysyy lukittuna, vaikka
   upstream/main muuttuisi sillä välin. Vaiheet ja kulunut aika näkyvät
   näkymän yläosassa. **Tekninen loki** näyttää työkalujen todelliset tulosteet.
5. **Peruuta toiminto** pysäyttää myös rakennuksen lapsiprosessit. Nykyinen
   ohjelma säilyy. Vain yksi backend-operaatio voi käyttää samaa työtilaa.
6. Kun näet **ASkompu-ydin valmis**, valitse **Käynnistä uudelleen** tai
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
sen `core-versions`-alihakemistoa. Polun voi antaa kehittäjän
`--core-home POLKU` -valitsimella. Työtilassa ovat Git-välimuisti,
operaatiokohtaiset lokit, erilliset rakennukset ja käynnistyshistoria.
Jokainen rakennus saa uuden tunnisteen sekä oman `core`, `source` ja `app`
-hakemistonsa. Vanha CMakeCache ei voi valita väärää ydintä.

Pakettiin sisältyy `simulator-source.zip` sekä `tools/core_versions.py`.
Lähdepaketti sisältää simulaattorin, esityssovittimen, testit ja lisenssin;
valittu tuotantokoodi kloonataan erikseen. Tämä toimii myös paikallisesti
tehdystä, vielä pushaamattomasta simulaattoriversiosta. Lähdepaketti kopioidaan
uuteen rakennukseen, joten käyttäjän työpuuta ei muokata.

Backend rakentaa, ajaa CTestin, asentaa erilliseen `app`-hakemistoon,
tarkistaa uuden ohjelman ilmoittaman SHA:n ja ajaa SDL-dummy-GUI-testin.
Vasta sitten se kirjoittaa valmiin rakennuksen tiedot ja EXEn SHA-256:n.
Käynnistyksessä tiiviste tarkistetaan uudelleen. Windows-asennus käyttää
edelleen staattisen runtime-paketin DLL-auditointia.

Backend käynnistää erillisen EXEn. Uusi ohjelma kirjoittaa yksilöllisen
kuittauksen ensimmäisen onnistuneesti piirretyn ruudun jälkeen. Vasta tämän
jälkeen GUI sulkee vanhan ikkunan. Puuttuva kuittaus, prosessin virhe tai
muuttunut EXE estää vaihdon. Edellisen ohjelman polku säilyy history.json:ssa.
CLI-käynnistys raportoi onnistumisen, mutta ei sulje käyttäjän muita ikkunoita.

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
```

Windowsissa käytä `py -3` ja Windows-polkuja; lähdepaketti on
`build/windows-release/simulator-source.zip`. `--job` määrää operaation
lokihakemiston. Luo sinne `cancel`-tiedosto CLI-operaation peruuttamiseksi.
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
