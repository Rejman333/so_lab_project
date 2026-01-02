### Dziennik

1. Ciekawy problem, korzystająć z WSL1 odkryłem, że `mq_open: Function not implemented` co wymusiło na mnei migrację na
   WSL2.
2. Okazłao się że grupy procesów spaliły mi cały dzień i były całkowicie nie potrzebne. Bo sygnały są przekazywane dziecia
3. Liczne problemy z droanmi w samej bazie
4. Nieoczekiwane problemy z  sygnałami, przerywajacymi oczekiwania na semafora
5. Zdecydowałem się, na przepisanie kodu, i wykorzystanie zmiennych globalnych by neico ułatwić pracę z kodem.




Pytania na psotkanie: 
- Kto dodaje obiekt dron do tablicy
- Radzenei sobie z segmentacją tablicy
- Czy moje logi są ok
- Jak odtworzyć 2 konsole dla grafiki/lub czy moge raylib?
- Kwestie _exit i innych dziwnych kwesti
- Czy tworzenei dronów musi być wątkiem
- Czy przejmować się deltą czasu
- Czy rbię coś złego z semaforami lub pamięcią wspułdzeloną
- Czy dodać jeszcze jakąś metodę komunikacji
- Czy idę w dobrą stronę a jeśli nie to dlaczego

tunele są dwa potoki paipy mkfifo pipe
