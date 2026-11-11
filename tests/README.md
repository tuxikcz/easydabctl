# Testy

`make test` spouští lokální mock HTTP server na 127.0.0.1:18765. V některých izolovaných prostředích nelze otevírat ani loopback sockety (`Operation not permitted`), takže test nebude možné provést bez síťových oprávnění. Testy neposílají nic na skutečný modulátor.
