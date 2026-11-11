# easydabctl — nouzové CLI pro EasyDAB v2

Minimální C++17 program (`libcurl`) pro stav a základní ovládání bez původního webu. Ověřeno vůči zdrojovému kódu web UI z archivu `easydabv2_sfn-28.11.2016.tar.gz`; skutečná deska má firmware `easydabv2.6144-28.11.2017`.

## Kompilace na Debianu / RPi

```sh
sudo apt install g++ make libcurl4-openssl-dev
make
sudo install -m 0755 easydabctl /usr/local/bin/easydabctl
```

## Příklady

```sh
easydabctl status
easydabctl --raw status
easydabctl --json status
easydabctl amplitude
easydabctl config
easydabctl --yes rf off
easydabctl --yes rf on
easydabctl --yes eti off
easydabctl --yes eti reconnect
easydabctl --yes eti client
easydabctl --yes eti server
easydabctl --yes amplitude set 122
easydabctl --yes restart
```

Výchozí host `10.26.76.125`, uživatel `admin`. Nastavit lze `--host`, `--user`, `--timeout` (1..30 s). Heslo se interaktivně zadává přes `/dev/tty` bez echo. Pro neinteraktivní použití lze `--password-env NAZEV_PROMENNE` (pozor: prostředí procesu může být dostupné privilegovaným uživatelům). Nevkládat heslo na příkazový řádek.

Příkazy měnící stav vyžadují `--yes`. Důležité: `rf on` může okamžitě aktivovat buzení PA; používejte správnou RF zátěž a kontrolu výkonu. `eti client` zapíná socket, **nikoliv** zapisuje TCP Client režim do flash. `eti reconnect` před zapnutím ověří, že konfigurace ve flash obsahuje TCP Client (`0x04`). Režim server/client pro `eti ...` musí odpovídat uložené konfiguraci.

`config` čte jen veřejné technické položky a nevypisuje přihlašovací údaje. Není implementován zápis do SPI flash, změna frekvence, ani jiné neověřené příkazy. `amplitude set` podporuje jen 1..255 (00 znamená čtení); změna je runtime, nikoli uložení do flash.

Po zápisových příkazech se čte stav, pokud to dává smysl. Návratový kód 0 znamená dokončený HTTP request (a případně následné úspěšné čtení), **ne fyzicky potvrzený RF výstup ani úspěch ZMQ**. Při chybě timeoutu zápis neopakujte naslepo: mohl být přijat.

Provozní zásady: nepoužívat souběžně s několika dalšími klienty bez koordinace; tento program nevyužívá zámek PHP aplikace. HTTP Basic Auth přes nešifrovanou lokální síť, používat jen v důvěryhodné/VPN síti.

## Testování

`make test` spustí mock HTTP server výhradně na 127.0.0.1. Ve striktním sandboxu bez povoleného socket API jej nelze spustit. Zdrojový kód byl zkompilován v x86_64 Linuxu; na RPi je nutné sestavit vlastní binárku.
