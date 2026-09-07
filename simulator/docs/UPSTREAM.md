# Upstreamin päivittäminen ja yhteensopivuus

Remotet:

- `origin`: `https://github.com/ppqalt/ASkompu-sim.git` — simulaattoriforkki.
- `upstream`: `https://github.com/Mikky100/ASkompu.git` — alkuperäinen ASkompu.

Tarkista `git remote -v`. Lisää upstream vain, jos se puuttuu:

```sh
git remote add upstream https://github.com/Mikky100/ASkompu.git
```

## Suositus: erillinen päivityshaara ja merge

Merge säilyttää forkkiin jo jaetut commitit ja upstream-historian, jolloin
muiden käyttäjien historiaa ei tarvitse kirjoittaa uudelleen. Älä päivitä
keskeneräiseen työpuuhun: tallenna oma työ hallitusti ensin.

```sh
git status
git fetch upstream
git log --oneline HEAD..upstream/main
# Anna päivityshaaralle oma kuvaava nimi; seuraava on esimerkki.
git switch -c paivitys/upstream-tarkistus
git merge --no-commit --no-ff upstream/main
```

Tarkista yhdistetty diff ja ratkaise mahdolliset ristiriidat. Jos et halua
jatkaa aloitettua mergeä, `git merge --abort` palauttaa sitä edeltäneen
puhtaan tilan. `fetch` päivittää vain paikalliset remote-viitteet; se ei
integroi lähteitä kehityshaaraan eikä työnnä mitään palvelimelle.

Aja päivityksen tarkistukset:

```sh
cd simulator
cmake --preset linux-release
cmake --build --preset linux-release
ctest --preset linux-release
./build/linux-release/askompu-simulaattori --tarkista
cd ..
pio run -e lilygo-main
pio run -e esp32s3-ili9488-main
```

Tarkista Windows-laatuportti ja tarvittaessa Windows 11:n manuaalilista.
Vasta tulosten tarkistamisen jälkeen kehittäjä päättää merge-commitista ja
päivityshaaran viemisestä origin-palvelimelle. Tässä ohjeessa tai työkaluissa
ei ole automaattista pushia. Upstreamiin ei kirjoiteta mitään.

## Tarkista yhdistettävyys muuttamatta omaa repoasi

Linuxilla, puhtaan kehityshaaran repositorion juuressa:

```sh
python3 simulator/tools/check_upstream.py
```

Työkalu lukee HEADin ja tarkistaa puhtaan työpuun. Se kloonaa lähderepon
järjestelmän väliaikaishakemistoon ilman yhteisiä objektitiedostoja,
noutaa `upstream/main`:n **vain tähän klooniin** ja yrittää
`merge --no-commit --no-ff` -yhdistämistä. Onnistuneessa yhdistämisessä se
rakentaa linux-release-presetin ja ajaa kaikki moottori-, ohjain- ja
host-testit sekä rakentaa GUI:n. Valinnainen `--firmware` lisää molemmat
PlatformIO-rakennukset. `--merge-only` tarkistaa pelkän Git-yhdistettävyyden,
ei API-yhteensopivuutta tai testituloksia.

Lähderepon HEAD, työpuu, haarat ja remotet pysyvät ennallaan. Tilapäinen
klooni poistetaan myös virheessä. Lokissa ovat molempien lähteiden
commit-tunnisteet, ristiriitatiedostot tai epäonnistunut rakennus/testi.
Likaista työpuuta ei ohiteta hiljaa: työkalu kieltäytyy, jotta tuloksen ei
luulla koskevan keskeneräisiä muutoksia.

GitHub Actionsin **Upstream-yhteensopivuus (tilapäinen klooni)** tekee saman
`workflow_dispatch`-käynnistyksestä valitulle haaralle. Lisää workflow ensin
forkkiin normaalilla, kehittäjän hyväksymällä commit/push-työnkululla.
Työ käyttää vain lukuoikeutta eikä tallenna checkout-tunnuksia. Se ei luo
PR:ää, committia, julkaisua tai ajastettua automaattista yhdistämistä.
Työn käynnistys on tietoinen valinta; sen sisäiset tarkistukset ovat automaattiset.

## Pieni yhteensopivuusraja

`SimulatorEngine` kääntää ja kutsuu edelleen suoraan repon tuotantokoodia.
Se sisältää ajan, pulssien ja painikkeiden integraation. `AppController`
keskittää GUI:n ApplicationCore-, kello-, kilpailu-, kalibrointi- ja
varastokutsut. Diagnostiikkarivit muodostetaan kysyttäessä; mitään rinnakkaista
tuotantotilaa tai kilpailulaskentaa ei ylläpidetä.

`SimulatorView` käyttää DisplayModelia, ohjaimen lukumetodeja ja vakaata
moottorirajapintaa. `DeviceDisplay` on tarkoituksella suoraan sidoksissa
DisplayModeliin ja tuotannon näyttöapuihin: mallin muuttuessa piirto pitää
voida päivittää näkyvästi. `Finnish` muotoilee tuotannon enumit ja tapahtumat.
Testit saavat lukea tuotantoa suoraan, jotta ne tarkistavat sovittimen tuloksen
riippumattomasti. Täyttä API-muutosten eristystä ei luvata.

Tuotannosta ei ole kopioita `simulator/`:ssa. Sisäiset tuotantomuutokset
siirtyvät simulaattoriin uudelleenrakennuksessa. Vaiheen 2 yhteinen
`DisplayFormatting.h` säilyy; vaiheessa 2.5 korjattiin vain neljän
printf-argumentin tyyppi vastaamaan PRIu64-muotoilua. Muutokset tuotantopuihin
pidetään pieninä ja tarpeelliset upstream-ristiriidat ratkaistaan tarkoituksen
perusteella, ei kopioimalla vanhaa ydintä simulaattoriin.
