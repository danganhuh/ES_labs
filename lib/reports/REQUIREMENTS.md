Sisteme de operare pentru sisteme
încorporate
Scopul lucrării
aplicat,
Familiarizarea cu conceptele fundamentale ale sistemelor de operare pentru sisteme elec
tronice încorporate, precum s,
i cu implementarea s,
i utilizarea acestora în dezvoltarea
iilor pentru microcontrolere (MCU). Lucrarea urmăres,
te realizarea unei aplicat,
ii
care să ruleze secvent,
ial sau concurent mai multe task-uri, utilizând fie programare bare
metal, fie un sistem de operare în timp real (RTOS, ex. FreeRTOS). Aplicat,
ia trebuie să
evident,
ieze metodologia de planificare, sincronizare (model provider/consumer) s,
i execut,
ie
(preemptivă sau non-preemptivă) a task-urilor, cu documentarea clară a arhitecturii soft
ware s,
i a fluxului de date între task-uri.
Obiectivele lucrării
• Familiarizarea cu principiile de planificare s,
i execut,
ie a task-urilor într-un sistem em
bedded, atât în regim secvent,
ial (bare-metal), cât s,
i concurent (RTOS/FreeRTOS).
• Înt,
elegerea s,
i aplicarea recurentei s,
i offset-urilor pentru optimizarea utilizării proce
sorului.
• Implementarea mecanismelor de sincronizare s,
i comunicare între task-uri, inclusiv
modelul provider/consumer s,
i utilizarea bufferelor.
• Analiza avantajelor s,
i limitărilor execut,
iei secvent,
iale versus multitasking preemptiv.
• Documentarea arhitecturii software, prezentarea schemelor bloc s,
i a schemelor elec
trice ca parte a metodologiei de proiectare.
3.1 Analiza domeniului de aplicare
Sistemele de operare pentru sisteme electronice încorporate (embedded) sunt proiectate
pentru a gestiona resursele hardware limitate s,
i pentru a permite executarea eficientă a
permit,
aplicat,
iilor în timp real. Acestea trebuie să răspundă rapid la evenimente externe s,
i să
asigure o utilizare optimă a procesorului s,
i a memoriei. Sistemele de operare în timp
real (RTOS) oferă funct,
ionalităt,
i avansate de planificare s,
i sincronizare a task-urilor,
ând dezvoltatorilor să gestioneze eficient execut,
ia acestora. Sistemele bare-metal,
pe de altă parte, implică programarea directă a hardware-ului fără un sistem de operare
intermediar, oferind un control mai mare asupra resurselor, dar necesitând o gestionare
manuală a task-urilor s,
i a sincronizării. În această lucrare, vom explora ambele abordări,
comparând avantajele s,
i dezavantajele fiecărei metode.
25
26
Lucrarea de laborator nr. 3. Sisteme de operare pentru sisteme încorporate
3.1.1 Sisteme secvent,
iale bare-metal
Bare-metal reprezintă programarea directă a hardware-ului, fără suportul unui sistem
de operare, oferind control total asupra resurselor microcontrolerului. Această abordare
implică gestionarea manuală a execut,
iei s ,
i sincronizării task-urilor. Acest termen mai
poate fi exprimat ca "programare directă pe microcontroller" sau "execut,
ie fără sistem de
operare".
În sistemele secvent,
iale bare-metal, se pot realiza prin utilizarea unui timer al micro
controllerului configurat pentru a genera întreruperi periodice (de exemplu, la fiecare 1
ms) figura 3.1.
Figure 3.1: Diagrama flux: rutina de întrerupere pentru cu recurenta sys-tick
unei recurent,
Planificatorul de task-uri este invocat în rutina de întrerupere, care implementează
un mecanism simplu ce verifică s,
i execută funct,
iile principale ale task-urilor, conform
e configurate pentru fiecare task. Recurent,
a reprezintă intervalul de timp
(în tick-uri de timer) la care un task trebuie să fie executat.
Recurent,
a este realizată prin intermediul unui contor asociat fiecărui task, care este
decrementat la fiecare întrerupere. Când contorul ajunge la zero, task-ul este activat s,
i
executat, după care contorul este resetat la valoarea init,
ială specificată pentru recurent,
ă.
timp fat,
Offsetul reprezintă un decalaj în timp fat,
ă de pornirea sistemului. Offset-ul se foloses,
te
pentru planificarea primei iterat,
ii de execut,
ie a taskului, reprezentând un decalaj în
ă de pornirea sistemului. Offset-ul se realizează prin init,
ializarea contorului de
recurent,
ă cu această valoare la start, iar ulterior, la resetare, acesta preia valoarea con
f
igurată ca recurent,
ă. Astfel, fiecare task va fi activat la un moment specific în cadrul
execut,
iei.
Figure 3.2: Init,
ializarea offset-ului pentru fiecare task în planificatorul secvent,
ial
Ordinea executiei sau prioritatea task-urilor este determinată de ordinea în care sunt
verificate în planificator. Task-urile cu prioritate mai mare (verificate primele) vor fi exe
cutate înaintea celor cu prioritate mai mică. De exemplu, în cazul producator-consumator,
producatorul de informat,
ie trebuie apelat înaintea consumatorului, respectiv producătorul
3.1. Analiza domeniului de aplicare
27
va avea prioritate mai mare în cadrul aceluias,
i sistem tick.
O implementare simplificată a planificatorului secvent,
ial de task-uri este ilustrată în
f
igura 3.3.
Figure 3.3: Diagrama flux: planificator secvent,
ial de task-uri
cerint,
Taskurile aplicat,
iei au asociate câte o funct,
ie principală, o variabilă de stare care indică
dacă task-ul este activ s,
i un contor de recurent,
ă. Ordinea s,
i frecvent,
a apelării task-urilor
sunt controlate exclusiv de logica din rutina de planificator. Această metodă permite
multitasking simplu, fără complexitatea unui RTOS, fiind potrivită pentru aplicat,
ii cu
e predictibile de timp s,
i resurse limitate.
Implementarea planificatorului poate arăta ca în exemplul de cod din listingul 3.3.
Listing 3.1: Declarat,
ii s,
i incluziuni pentru planificatorul secvent,
ial de task-uri
1 #define REC_A 3 // recurenţa task A în ms
2 #define REC_B 2 // recurenţa task B în ms
3 #define REC_C 4 // recurenţa task C în ms
4
5 #define OFFS_A 3 // offset-ul task A în ms
6 #define OFFS_B 1 // offset-ul task B în ms
7 #define OFFS_C 0 // offset-ul task C în ms
8
9 int rec_cnt_A = 0;
10 int rec_cnt_B = 0;
11 int rec_cnt_C = 0;
12
13 // Prototipuri funcţii task
14 void Task_A(void);
15 void Task_B(void);
16 void Task_C(void);
Listing 3.2: Funct,
ia de setup pentru planificatorul secvent,
ial de task-uri
1 /**
28 Lucrareadelaboratornr.3. Sistemedeoperarepentrusistemeîncorporate
2 * Iniţializează offset-ul pentru fiecare task.
3 */
4 void os_seq_scheduler_setup(void) {
5 rec_cnt_A = OFFS_A; // initializează offset-ul pentru task A
6 rec_cnt_B = OFFS_B; // initializează offset-ul pentru task B
7 rec_cnt_C = OFFS_C; // initializează offset-ul pentru task C
8 }
Listing3.3:Funct,
iaprincipalădeplanificares,
iexecut,
ieatask-urilor
1 /**
2 * Planificatorul secvenţial: verifică şi execută fiecare task conform recurenţ
ei.
3 */
4 void os_seq_scheduler_loop(void) {
5 // Task A
6 if (--rec_cnt_A <= 0) {
7 rec_cnt_A = REC_A; // reiniţializează contorul de recurenţă
8 Task_A(); // execută task A
9 }
10
11 // Task B
12 if (--rec_cnt_B <= 0) {
13 rec_cnt_B = REC_B; // reiniţializează contorul de recurenţă
14 Task_B(); // execută task B
15 }
16
17 // Task C
18 if (--rec_cnt_C <= 0) {
19 rec_cnt_C = REC_C; // reiniţializează contorul de recurenţă
20 Task_C(); // execută task C
21 }
22 }
Sepoaterealizaovariantamai compactăfolosindstructuridecontextpentrufiecare
task,s,
iorganizareaaunortablouridestructuridecontext. vezivariantaoptimizatamai
josînlistingul3.4,3.6si3.7.
Listing3.4:Declarat,
iis,
i incluziunipentruplanificatorulsecvent,
ialdetask-uri (variantaoptimizată)
1 #include <avr/io.h>
2 #include <avr/interrupt.h>
3
4 // Definirea structurii pentru contextul unui task
5 typedef struct {
6 void (*task_func)(void); // pointer către funcţia task-ului
7 int rec; // recurenţa task-ului în ms
8 int offset; // offset-ul task-ului în ms
9 int rec_cnt; // contorul de recurenţă
10 } Task_t;
11
12 // Definirea ID-urilor pentru fiecare task
13 enum {
14 TASK_A_ID = 0,
15 TASK_B_ID,
16 TASK_C_ID,
17 MAX_OF_TASKS
18 };
19
20 #define REC_A 3 // recurenţa task A în ms
21 #define REC_B 2 // recurenţa task B în ms
22 #define REC_C 4 // recurenţa task C în ms
23
24 #define OFFS_A 3 // offset-ul task A în ms
25 #define OFFS_B 1 // offset-ul task B în ms
26 #define OFFS_C 0 // offset-ul task C în ms
27
28 // Prototipuri funcţii task (implementate separat)
3.1. Analizadomeniuluideaplicare 29
29 void Task_A(void);
30 void Task_B(void);
31 void Task_C(void);
32
33 // Definirea task-urilor cu recurenţă şi offset (cu initializare implicită)
34 Task_t tasks[MAX_OF_TASKS] = {
35 {Task_A, REC_A, OFFS_A, 0}, // Task A cu recurenţa de 3 ms si offset 3ms
36 {Task_B, REC_B, OFFS_B, 0}, // Task B cu recurenţa de 2 ms si offset 1ms
37 {Task_C, REC_C, OFFS_C, 0} // Task C cu recurenţa de 4 ms si offset 0ms
38 };
Listing3.5:Funct,
iedeinit,
ializareaunui task
1 /**
2 * Iniţializează structura descriptor al task-ului cu funcţia, recurenţa şi
offset-ul specificate.
3 */
4 void os_seq_scheduler_task_init(Task_t* task, void (*task_func)(void), int rec,
int offset) {
5 task->task_func = task_func; // asociază funcţia task-ului
6 task->rec = rec; // setează recurenţa task-ului
7 task->offset = offset; // setează offset-ul task-ului
8 task->rec_cnt = offset; // iniţializează contorul de recurenţă cu offset
ul
9 }
Listing3.6: Init,
ializeazăplanificatoruldetask-uri
1 /**
2 * Iniţializează fiecare task cu funcţia, recurenţa şi offset-ul specific.
3 */
4 void os_seq_scheduler_setup(void) {
5 os_seq_scheduler_task_init(&tasks[TASK_A_ID], Task_A, REC_A, OFFS_A);
6 os_seq_scheduler_task_init(&tasks[TASK_B_ID], Task_B, REC_B, OFFS_B);
7 os_seq_scheduler_task_init(&tasks[TASK_C_ID], Task_C, REC_C, OFFS_C);
8 }
Listing3.7:Planificatoruldetask-uri
1 /**
2 * Planificatorul secvenţial: verifică şi execută fiecare task conform recurenţ
ei.
3 */
4 void os_seq_scheduler_loop(void) {
5 for (int i = 0; i < MAX_OF_TASKS; i++) {
6 if (--tasks[i].rec_cnt <= 0) {
7 tasks[i].rec_cnt = tasks[i].rec; // reiniţializează contorul de recurenţă
8 tasks[i].task_func(); // execută task-ul
9 }
10 }
11 }
Listing3.8: Init,
ializareatimeruluipentruîntreruperide1ms
1 /**
2 * Iniţializarea timerului 1 pentru întreruperi de 1 ms pentru
microcontrollerul AVR.
3 */
4 void timer1_init(void) {
5 // configurare timer 1 pentru a genera întreruperi la fiecare 1 ms
6 TCCR1A = 0; // setare mod normal
7 TCCR1B = (1 << WGM12) | (1 << CS11); // setare prescaler 8
8 OCR1A = 124; // valoare comparare pentru 1ms
9 TIMSK1 = (1 << OCIE1A); // activare întrerupere pe comparare
10 }
Listing3.9:Rutinadeîntreruperepentrutimerul1
30
Lucrarea de laborator nr. 3. Sisteme de operare pentru sisteme încorporate
1 /**
2
3
* Rutina de întrerupere pentru timerul 1: invocă planificatorul de task-uri.
*/
4 ISR(TIMER1_COMPA_vect) {
5
6 }
os_seq_scheduler_loop();
Sunt posibile optimizări suplimentare, precum implementarea unui mecanism de sleep
pentru dezactivarea task-urilor sau delay- ret,
inerea activităt,
ii pentru o perioadă specifi
cată. Astfel, task-urile nu consumă resurse inutil când nu sunt active.
Unmecanism de wake-up din regimul de sleep/delay se activează la aparit,
ia unui eveni
ment definit (semnal de wake-up), legat la task printr-o referint,
ă, pe care planificatorul îl
monitorizează pentru taskul respectiv. La identificarea semnalului, se resetează flagurile
de sleep s ,
i contorul de delay. Acest mecanism poate servi ca bază pentru event-driven
programming, activând taskurile din sleep la aparit,
ia unui eveniment s,
i revenind în sleep
după procesarea acestuia.
Aceste optimizări presupun dezvoltarea unui API pentru gestionarea taskurilor, cu
funct,
ii de creare, s,
tergere, sleep, delay, wake-up etc. Optimizările implică utilizarea unor
structuri de date extinse pentru gestionarea taskurilor sau implementarea unor algoritmi
specifici funct,
ionalităt,
ii propuse.
O referint,
ă la un sistem de operare secvent,
ial industrial este OSEK-VDX, popular în
sectorul automotive pentru aplicat,
ii în timp real s,
i gestionare eficientă a hardware-ului.
Detalii despre acest sistem pot fi găsite la [21]
Taskuri non-preemptive subînt,
eleg că odată ce un task începe să ruleze, nu poate
f
i întrerupt până când nu se termină. Tascurile non-preemptive se numesc taskuri cu
cedare benevolă a resurselor. În sistemele secvent,
iale bare-metal, toate taskurile sunt
non-preemptive, fapt pentru care este necesar să se asigure ca fiecare task să ruleze într
un timp cât mai scurt pentru a nu impacta celelalte taskuri. Acest fapt presupune inclusiv
reducerea până la excludere a spin-lock-urilor sau a buclelor de as,
teptare (busy-waiting)
în cadrul taskurilor, care pot duce la blocarea întregului sistem.
Spin-lock-urile sunt mecanisme de sincronizare care implică bucle de as,
teptare activă
(busy-waiting) pentru a obt,
ine accesul la o resursă partajată. În sistemele secvent,
iale
bare-metal, utilizarea spin-lock-urilor poate duce la blocarea întregului sistem, deoarece
un task care det,
ine un spin-lock poate bloca execut,
ia altor taskuri.
În cazul în care există taskuri care prezintă riscul de blocare la verificarea unei condit,
ii
(de exemplu, as,
teptarea apăsării unui buton), este recomandată reproiectarea logicii
taskului astfel încât bucla de as,
teptare (busy-waiting) să fie înlocuită cu o verificare simplă
a condit,
iei s ,
i returnarea din funct,
ie dacă aceasta nu este îndeplinită. Astfel, taskul va re
lua verificarea la următoarea activare, evitând blocarea execut,
iei globale. Acest principiu
este ilustrat în Figura 3.4.
3.1. Analiza domeniului de aplicare
31
(a) Execut,
ie secvent,
ială cu spin-lock (busy
waiting)
(b) Execut,
ie secvent,
ială fără spin-lock (non
blocking)
Figure 3.4: Convertirea unui task cu spin-lock într-un task non-blocking
Planificarea execut,
iei taskurilor secvent,
iale
de evolut,
Se realizează prin stabilirea recurent,
ei s,
i offset-ului pentru fiecare task, cât s,
i ordinea în
care acestea sunt executate. Recurent,
a taskului este stabilită din considerentele de viteză
ie a proceselor pe care le gestionează a metodelor de prelucrare a semnalului, s,
i
cât de critic este acel proces din punct de vedere temporal.
Stabilirea de recurent,
e s ,
i offset-uri adecvate este necesară pentru a asigura o utilizare
optimă a procesorului s,
i pentru a evita suprasolicitarea acestuia. În cazul limita în care
toate taskurile au recurent,
ă de 1 ms s,
i offset 0, toate taskurile se vor executa în fiecare tick,
ceea ce poate duce la supraîncărcarea procesorului s,
i la întârzieri în execut,
ia taskurilor.
Acest lucru este ilustrat în Figura 3.5.
Figure 3.5: Execut,
ia task-urilor secvent,
iale cu recurent,
ă de 1ms
Pentru a evita acest lucru, este recomandat să se stabilească recurent,
e s ,
i offset-uri
diferite pentru fiecare task, în funct,
ie de necesităt,
ile aplicat,
iei.
De exemplu, pentru un task care gestionează apăsarea unui buton, se va lua în con
siderare viteza maximă de apăsare a unui buton, care este de aproximativ 5 apăsări pe
secundă, ceea ce înseamnă o recurent,
ă minimă de 200 ms. Recurent,
a se măres,
te în cazul
în care informat,
ia pe care o gestionează va necesita o prelucrare. De exemplu, pentru
cazul butonului va fi necesară o prelucrare de debounce, care va implica maturarea sem
nalului timp de 5 citiri consecutive de aceeas,
i valoare, ceea ce va implica o cres,
tere a
recurent,
ei până la 40 ms. Considerând că un buton se poate apăsa în anumite cazuri mai
repede de 200 ms, se poate stabili o recurent,
ă de 20 ms, pentru a asigura citirea corectă
a stării butonului.
În mod similar, pentru un task care gestionează un senzor de temperatură, se poate
stabili o recurent,
ă mai mare, de exemplu 1 s, deoarece temperatura nu se schimbă foarte
rapid. Utilizarea unor metode de filtrare, cum ar fi filtrarea zgomotului uniform sau im
32
Lucrarea de laborator nr. 3. Sisteme de operare pentru sisteme încorporate
pulsionar, presupune colectarea unui număr mai mare de citiri, de exemplu pe o fereastră
de 8 citiri. Acest lucru ajută la filtrarea zgomotului s,
i la obt,
inerea unei valori mai precise.
Astfel, recurent,
a poate fi mărită, iar 100 ms reprezintă un compromis bun între viteza de
răspuns s,
i precizia măsurătorii.
Definind recurent,
e relaxate pentru fiecare task, se poate asigura că nu toate taskurile
se vor executa în acelas,
i tick, evitând astfel supraîncărcarea procesorului. Acest lucru
este ilustrat în Figura 3.6.
Figure 3.6: Execut,
ia task-urilor secvent,
iale cu recurent,
e diferite
Offsetul se stabiles,
te în funct,
ie de recurent,
a taskului s,
i de ordinea în care acesta trebuie
să fie executat. De exemplu, dacă un task are nevoie de informat,
ii de la un alt task, care
are nevoie de 20 ms pentru ca datele pe care le produce să fie valide, atunci offsetul pentru
primul task ar trebui să fie setat la minim 20 ms.
Un alt factor de select,
ie a offsetului este timpul de execut,
ie al taskului. Suma tuturor
tascurilor care se execută într-un sistem tick trebuie să fie mai mică decât durata unui
tick. Respectiv, la planificarea recurent,
ei trebuie să se t,
ină cont s,
i de timpul de execut,
ie al
f
iecărui task, pentru a distribui corect execut,
ia tascurilor în timp s,
i a nu suprasolicita un
anumit tick. Pentru calcul se foloses,
te utilizarea CPU (CPU load), care se calculează
ca
CPU load = timpul de execut,
ie total
f
ie ment ,
×100%
(3.1)
durata tick-ului
unde timpul de execut,
ie total este suma timpilor de execut,
ie ai tuturor task-urilor s,
i
durata tick-ului este intervalul de timp între două întreruperi consecutive de timer. Pentru
a asigura o funct,
ionare stabilă a sistemului, CPU utilization în fiecare tick ar trebui să
inută sub 100%, ideal sub 70-80%, pentru a permite s,
i alte procese de sistem să
ruleze fără întârzieri.
În cazul în care se identifică suprasolicitarea unui tick, se poate interveni prin modi
f
icarea offsetului taskurilor, astfel încât să se reducă numărul de taskuri care se execută
în acelas,
i tick. De exemplu, dacă două taskuri cu offseturi diferite se execută în acelas,
i
tick, se poate ajusta offsetul unuia dintre ele pentru a evita suprapunerea. În planifi
carea execut,
iei din figura 3.7, s-a reus,
it reducerea până la un task per tick în majoritatea
tick-urilor, prin ajustarea offseturilor taskurilor.
Figure 3.7: Execut,
ia task-urilor secvent,
iale cu offset-uri diferite
În concluzie, pentru reducerea de CPU Load se poate interveni s,
i prin mărirea recurent,
ei
taskurilor s,
i prin ajustarea offseturilor, astfel încât să se evite suprapunerea execut,
iei aces
3.1. Analiza domeniului de aplicare
33
tora în acelas,
i tick. Acest lucru ajută la distribuirea mai uniformă a sarcinii pe procesor
s ,
i la ment,
inerea unei utilizări eficiente a CPU-ului.
Sincronizarea s,
i comunicarea între taskuri în sistemele secvent,
iale non-preemptive
se realizează prin simple interfet,
e de comunicare, cum ar fi variabile globale sau structuri
de date partajate implementate prin funct,
ii tip setter s,
i getter.
Riscul de corupere a datelor în cazul accesului concurent la resurse partajate nu există
în cazul sistemelor secvent,
iale non-preemptive, deoarece taskurile nu sunt întrerupte în
timpul accesului la resurse partajate, iar toate operat,
iile de citire s,
i scriere se finalizează
fără a fi întrerupte.
Except,
ia ar putea fi cazurile de acces concurent la periferice hardware, fără a t,
ine cont
de starea acestora- flaguri de finalizare a operat,
iilor, cât s,
i cazul în care se rulează pe
un microcontroller cu arhitectură multi-core, unde taskurile pot rula pe nuclee diferite.
Pot apărea situat,
ii de acces concurent la resursele partajate, ceea ce ar duce la coruperea
datelor. Respectiv, ar fi nevoie de mecanisme de sincronizare mai complexe, cum ar fi
mutex-uri sau semafoare, pentru a preveni aceste situat,
ii.
3.1.2 Sisteme de operare RTOS
RTOS (Real-Time Operating System) este un sistem de operare specializat pentru ges
tionarea sarcinilor în timp real. Sistemele de operare RTOS permit execut,
ia multitasking
preemptivă, unde mai multe taskuri pot rula concurent, iar sistemul de operare gestionează
planificarea, sincronizarea s,
i alocarea resurselor. În RTOS, fiecare task are o prioritate s,
i
poate fi întrerupt (preemptat) de un alt task cu prioritate mai mare, asigurând răspuns
rapid la evenimente critice.
Unul din sistemele de operare populare open source pentru sisteme electronice în
corporate este FreeRTOS, care oferă o gamă largă de funct,
ionalităt,
i pentru gestionarea
taskurilor, inclusiv planificare preemptivă, semafoare, mutex-uri, cozi de mesaje s,
i timer
e software. FreeRTOS este proiectat pentru a fi us,
or de utilizat s,
i integrat în aplicat,
ii
embedded, oferind un echilibru între performant,
ă s,
i complexitate.
Comoditatea utilizarii FreeRTOS constă în faptul că oferă o interfat,
ă simplă s,
i clară
pentru crearea s,
i gestionarea taskurilor, precum s,
i pentru sincronizarea s,
i comunicarea
între acestea.
Programarea concurentă cu mai multe taskuri este mult mai simplă s,
i mai sigură decât
în cazul programării bare-metal, deoarece FreeRTOS gestionează automat planificarea s,
i
invocarea taskurilor, reducând riscul de erori umane în gestionarea acestora.
Fiecare dintre taskuri se va executa într-un context propriu, cu stiva s,
i resursele alo
cate, ceea ce izolează taskurile între ele, creând impresia că fiecare task rulează pe un
microcontroller dedicat, fără să t,
ină cont de existent,
a altor taskuri, dacă acesta nu are
nevoie de interact,
iune cu ele.
34
Lucrarea de laborator nr. 3. Sisteme de operare pentru sisteme încorporate
Figure 3.8: Diagrama flux: execut,
ia taskurilor concurente în RTOS
În RTOS, fiecare task este definit printr-o funct,
ie care cont,
ine logica specifică a taskului
s ,
i arată ca în listingul 3.10.
Listing 3.10: Exemplu de cod: definit,
ia unui task în FreeRTOS
1 void Task_A_RTOS(void *pvParameters) {
2
3
4
5
6
7
8 }
// Iniţializări specifice task-ului A
// ...
while (1) {
// Logica specifică a task-ului A
// ...
}
funct,
Pentru a lansa taskurile în FreeRTOS, este necesar să se creeze fiecare task folosind
ia xTaskCreate(), care alocă resursele necesare s,
i înregistrează taskul în sistemul de
operare. De asemenea, este necesar să se pornească planificatorul RTOS folosind funct,
ia
vTaskStartScheduler(), care începe execut,
ia taskurilor concurente. Acest lucru este
ilustrat în listingul 3.11.
Listing 3.11: Exemplu de cod: lansarea taskurilor în FreeRTOS
1
2
3
4
5
6
7
8
9
10
11
12
13
void main(void) {
// Iniţializări hardware şi alte iniţializări
// ...
// Crearea taskurilor
xTaskCreate(Task_A_RTOS, "Task A", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
xTaskCreate(Task_B_RTOS, "Task B", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
xTaskCreate(Task_C_RTOS, "Task C", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
// Pornirea planificatorului RTOS
vTaskStartScheduler();
// Schedulerul nu ar trebui să revină niciodată aici
14 for (;;);
15 }
Odată ce planificatorul RTOS a fost pornit, acesta va gestiona execut,
ia taskurilor conform
priorităt,
ilor s,
i politicilor de planificare definite. Fiecare task va fi programat să ruleze în
funct,
ie de timpul său de execut,
ie s,
i de resursele disponibile.
3.1. Analizadomeniuluideaplicare 35
Sistemesecvent,
ialecuRTOS
Reies,
inddindefinit,
iasistemelorsecvent,
iale,careexecutătaskurileperiodiccuorecurent,
ă
s ,
i unoffset, sepoate implementaunsistemsecvent,
ial folosindRTOS, princreareade
taskuricurecurent,
ăs,
ioffsetdiferite,caresărulezeconcurent.
Pentrua implementamecanismuldeoffset, sepoateutilizaoret,
ineredeexecut,
iea
bucleitaskuluipânălaexpirareaoffsetului, folosindfunct,
iavTaskDelay(portTickType
xTicksToDelay).
Această funct,
iepunetaskul înstareadeas,
teptare(blockedstate)pentruunnumăr
specificatdetick-uri, eliberândCPUpentrualtetaskuri. Astfel, taskulnuvaconsuma
resurseCPUîntimpulperioadeideas,
teptare,permit,
ândaltortaskurisăruleze.
Pentruaimplementarecurent,
a,sepoateutilizaaceeas,
i funct,
ievTaskDelay(),pentru
aintroduceoîntârziereîntreexecut,
iilesuccesivealetaskului.Darînacestcaz, întârzierea
trebuiesăfiecalculatăastfelîncâtsăseasigurecătaskulsevaexecutalaintervaleregulate,
conformrecurent,
eispecificateconsiderandsitimpuldeexecutieataskului.
Ometodămai eficientă este folosirea funct,
iei vTaskDelayUntil( portTickType*
pxPreviousWakeTime,portTickType xTimeIncrement ), careasigurăcătaskul seva
trezi la intervaleregulate, indiferentdetimpuldeexecut,
ieal taskului. Aceastăfunct,
ie
iacaparametruunpointer lavariabilacareret,
inetimpulultimei treziriataskului s,
iun
incrementdetimp.Astfel,taskulvafiprogramatsărulezelaintervaleregulate,conform
recurent,
eispecificate, fărăafiafectatdevariat,
iileîntimpuldeexecut,
ie.
Oimplementaresimplificatăaunuitasksecvent,
ialînFreeRTOSutilizândacestefunct,
ii
esteilustratăînlistingul3.12.
Listing3.12:Exempludecod: definit,
iaunui tasksecvent,
ial înFreeRTOS
1 #define REC_A 3 // recurenţa task A în ms
2 #define OFFSET_A 1 // offset-ul task A în ms
3
4 void Task_A_RTOS(void *pvParameters) {
5
6 // Iniţializări specifice task-ului A
7 // ...
8
9 // Convertirea offset-ului şi recurenţei în tick-uri
10 const portTickType offsetTicks = pdMS_TO_TICKS(OFFSET_A);
11 const portTickType recTicks = pdMS_TO_TICKS(REC_A);
12
13 // Iniţializarea variabilei pentru timpul ultimei treziri
14 portTickType xLastWakeTime;
15
16 // Aşteaptă offset-ul iniţial
17 vTaskDelay(offsetTicks);
18
19 // Obţine timpul curent
20 xLastWakeTime = xTaskGetTickCount();
21
22 for (;;) {
23
24 // Execuţia taskului secvential cu logica specifică a task-ului A
25 Task_A();
26
27 // Aşteaptă următoarea trezire
28 vTaskDelayUntil(&xLastWakeTime, recTicks);
29 }
30 }
Datfiindfaptul căFreeRTOSesteunsistemdeoperarepreemptiv, cepresupunecă
taskurilepotfiîntrerupte înoricemomentdecătrealtetaskuri cuprioritatemaimare,
este important săseacordeatent,
ie laproiectareataskurilor secvent,
ialepentruaevita
36
Lucrarea de laborator nr. 3. Sisteme de operare pentru sisteme încorporate
problemele de sincronizare s,
i a asigura o execut,
ie corectă. de exemplu a se utiliza priorităt,
i
egale pentru toate taskurile secvent,
iale, pentru a preveni întreruperile nedorite.
În cazul FreeRTOS nu mai putem evita accesul concomitent la resurse partajate,
deoarece taskurile pot fi întrerupte în orice moment. Respectiv, este necesar să se utilizeze
mecanisme de sincronizare, cum ar fi mutex-uri sau semafoare pentru a proteja resursele
partajate s,
i a preveni coruperea datelor.
Sisteme concurente cu RTOS
Sistemele concurente cu RTOS permit execut,
ia simultană a mai multor taskuri, fiecare
având propriile sale cerint,
e de timp s,
i resurse. În acest context, FreeRTOS oferă mecan
isme eficiente pentru gestionarea concurent,
ei, cum ar fi planificarea bazată pe prioritate,
semafoare, mutex-uri s,
i cozi de mesaje.
Semafoarele sunt utilizate pentru a sincroniza taskurile, acestea reprezentând puncte
de sincronizare. cea mai simplă formă de semafor este un flag binar, care poate fi setat
sau resetat de către un task, iar alte taskuri pot as,
tepta până când semaforul este setat
pentru a continua execut,
ia.
as ,
De exemplu, un task identifica anumite situat,
ii s ,
i setează un semafor, iar un alt task
teaptă semaforul pentru a continua execut,
ia, implementând astfel un mecanism de
programare bazat pe evenimente.
vezi exemplul din listingul 3.13.
Listing 3.13: Exemplu de cod: utilizarea semafoarelor în FreeRTOS
1 #include "FreeRTOS.h"
2 #include "semphr.h"
3 SemaphoreHandle_t xSemaphore;
4 void Task_A(void *pvParameters) {
5
6
7
8
9
10
11 }
for (;;) {
// Logica specifică a task-ului A
// ...
// Setează semaforul pentru a semnala un eveniment
xSemaphoreGive(xSemaphore);
}
12 void Task_B(void *pvParameters) {
13
14
15
16
17
18
19
20 }
for (;;) {
// Aşteaptă semaforul pentru a continua execuţia
if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE) {
// Logica specifică a task-ului B după ce semaforul este luat
// ...
}
}
Figure 3.9: Mecanismul de sincronizare între taskuri cu semafor binar în RTOS
Mecanismul de semafoare este pe larg descris în documentat,
ia FreeRTOS, vezi referint,
a [11].
3.1. Analizadomeniuluideaplicare 37
Mutex-urile suntutilizatepentruaprotejaresurselepartajate, acesteaasigurândcă
doar unsingur taskpoate accesao resursă launmoment dat. Acestmecanismeste
recomandatîninteract,
iuneaîntremaimultetaskuriprininterfet,
etipsetters,
igetter.De
exemplutaskulproducătorde informat,
ievaapela interfat,
asetterpentruaactualizao
variabilăglobală, iar taskul consumatorvaapela interfat,
agetterpentruacitivaloarea
variabilei.Pentruaprevenicorupereadatelor,ambeleinterfetevorfolosiunmutexpentru
aprotejaaccesul lavariabilaglobala.
Setteriis,
igetteriicumutexsuntilustrat,
i înlistingul3.14.
Listing3.14:Exempludecod: utilizareamutex-urilorînFreeRTOS
1 #include "FreeRTOS.h"
2 #include "semphr.h"
3 SemaphoreHandle_t xMutex;
4 int sharedVariable;
5 void setSharedVariable(int value) {
6 // Blochează mutex-ul înainte de a accesa resursa partajată
7 xSemaphoreTake(xMutex, portMAX_DELAY);
8 sharedVariable = value; // Actualizează variabila partajată
9 // Eliberează mutex-ul după ce a terminat accesul
10 xSemaphoreGive(xMutex);
11 }
12 int getSharedVariable(void) {
13 int value;
14 // Blochează mutex-ul înainte de a accesa resursa partajată
15 xSemaphoreTake(xMutex, portMAX_DELAY);
16 value = sharedVariable; // Citeşte variabila partajată
17 // Eliberează mutex-ul după ce a terminat accesul
18 xSemaphoreGive(xMutex);
19 return value;
20 }
21
22 void Task_Producer(void *pvParameters) {
23
24 // initializarea semaforului
25 xMutex = xSemaphoreCreateMutex();
26
27 for (;;) {
28 // Logica specifică a task-ului producător
29 int newValue = ...; // Generează o nouă valoare
30 setSharedVariable(newValue); // Actualizează variabila partajată
31 }
32 }
33
34 void Task_Consumer(void *pvParameters) {
35 for (;;) {
36 // Logica specifică a task-ului consumator
37 int value = getSharedVariable(); // Citeşte valoarea variabilei
partajate
38 // Procesează valoarea citită
39 ...
40 }
41 }
38
Lucrarea de laborator nr. 3. Sisteme de operare pentru sisteme încorporate
Figure 3.10: Mecanismul de protect,
ie a resurselor partajate cu mutex
Mechanismul de mutex-uri este pe larg descris în documentat,
ia FreeRTOS, vezi referint,
a [13].
Cozi de mesaje (queues) sunt utilizate pentru a facilita comunicarea între taskuri,
permit,
ând unui task să trimită date către alt task prin intermediul unei cozi. Acest
mecanism este util pentru transmiterea de mesaje sau date între taskuri fără a necesita
acces direct la resurse partajate.
s ,
Acest mecanism este util în comunicarea asincronă între taskuri, unde un task poate
produce date la un ritm diferit fat,
ă de cel la care un alt task le consumă. De exemplu,
un task producător poate citi date de la o interfat,
ă hardware, cum ar fi portul serial, s,
i
le poate trimite printr-o coadă, iar un alt task consumator poate prelua datele din coadă
i le poate procesa.
Un exemplu de utilizare a cozilor de mesaje este ilustrat în listingul 3.15.
Listing 3.15: Exemplu de cod: utilizarea cozilor de mesaje în FreeRTOS
1 #include "FreeRTOS.h"
2 #include "queue.h"
3
4 QueueHandle_t xQueue;
5
6 void Task_Producer(void *pvParameters) {
7
8
9
10
11
12
13
14
15 }
16
int valueToSend = 0;
for (;;) {
// Generează o nouă valoare
valueToSend++;
// Trimite valoarea prin coadă
xQueueSend(xQueue, &valueToSend, portMAX_DELAY);
vTaskDelay(pdMS_TO_TICKS(100));
}
17 void Task_Consumer(void *pvParameters) {
18
19
20
21
22
23
24
25
26 }
int receivedValue;
for (;;) {
// Aşteaptă să primească o valoare din coadă
if (xQueueReceive(xQueue, &receivedValue, portMAX_DELAY) == pdPASS) {
// Procesează valoarea primită
...
}
}
3.2. Sarcina de laborator
39
Figure 3.11: Mecanismul de comunicare între taskuri cu cozi de mesaje în RTOS
Mecanismul de cozi de mesaje este pe larg descris în documentat,
ia FreeRTOS, vezi
referint,
a [12].
Dacă în cazul sistemelor secvent,
iale bare-metal nu problema cea mai mare de blocare
a sistemului o reprezintă spin-lock-urile, pentru că blocarea unui task neinteruptibil bloca
execut,
ia întregului sistem. În cazul RTOS, spin-lock-urile nu mai sunt la fel de relevante,
deoarece afectează doar taskul care le det,
ine.
Însă totus,
i există riscuri de blocare a sistemului, care pot apărea în cazul utilizării
mecanismelor de sincronizare s,
i comunicare între taskuri. Printre aceste tipuri de blocare
se regăsesc live-lock-urile s,
i dead-lock-urile.
f
iecare react,
Live-lock-urile apar atunci când două sau mai multe taskuri sunt blocate reciproc,
iunile celuilalt fără a progresa efectiv. În RTOS, live-lock-ul
ionând la act,
poate apărea dacă taskurile react,
ionează continuu la semnale sau mesaje, dar nu reus,
esc
să finalizeze operat,
iile, de exemplu, în cazul unor protocoale de comunicare defectuos
implementate.
f
iecare as,
Dead-lock-urile apar atunci când două sau mai multe taskuri sunt blocate reciproc,
teptând ca celălalt să elibereze o resursă. În RTOS, dead-lock-ul poate apărea
dacă taskurile det,
in resurse multiple s,
i as,
teaptă eliberarea acestora într-o ordine circulară,
blocând complet execut,
ia.
Diferent,
a dintre live-lock s,
i dead-lock în contextul RTOS constă în faptul că în live
lock, taskurile continuă să ruleze dar nu progresează, în timp ce în dead-lock, taskurile
sunt complet blocate s,
i nu mai pot fi planificate de sistemul de operare. RTOS oferă
mecanisme precum semafoare, mutex-uri s,
i time-out-uri pentru a preveni aceste situat,
ii
s ,
i pentru a asigura funct,
ionarea corectă a aplicat,
iilor concurente.
Aceste tipuri de blocaje pot fi prevenite printr-o proiectare atentă a sistemului, inclusiv
prin utilizarea corectă a mecanismelor de sincronizare s,
i comunicare, corect proiectate s,
i
justificate.
3.2 Sarcina de laborator
s ,
Sistem de monitorizare a duratei apăsărilor butonului cu semnalizare vizuală
i raportare periodică. Proiectat,
i s ,
i implementat,
i o aplicat,
ie multitasking pentru mi
crocontroler care să monitorizeze durata fiecărei apăsări a unui buton, să semnalizeze
vizual durata apăsării prin LED-uri s,
i să raporteze periodic statistici către utilizator prin
STDIO.
Aplicat,
ia va fi structurată în cel put,
in 3 task-uri distincte:
• Task 1- Detectare s,
i măsurare durată apăsare: Monitorizează starea bu
tonului, detectează tranzit,
iile apăsat/eliberat s,
i măsoară durata fiecărei apăsări (în
40
Lucrarea de laborator nr. 3. Sisteme de operare pentru sisteme încorporate
milisecunde). La fiecare apăsare validă, salvează durata într-o variabilă globală s,
i
semnalizează vizual apăsarea scurtă (< 500 ms) sau lungă (≥ 500 ms) prin aprinderea
unui LED verde (scurtă) sau ros,
u (lungă).
• Task 2- Contorizare s,
i statistici apăsări: La fiecare apăsare detectată, incre
mentează un contor global de apăsări, actualizează suma totală a duratelor scurte s,
i
lungi, precum s,
i numărul de apăsări scurte s,
i numărul de apăsări lungi. Realizează
un blink rapid al LED-ului galben la fiecare apăsare: 5 blicuri pentru scurtă s,
i 10
pentru lungă.
• Task 3- Raportare periodică: La intervale de 10 secunde, transmite utilizatoru
lui prin STDIO: numărul total de apăsări, numărul de apăsări scurte (< 500 ms),
numărul de apăsări lungi, durata medie a apăsărilor s,
i resetează statisticile.
3.2.1 Partea 1- Sisteme de operare Non-Preemptive (bare-metal)
Implementat,
i sarcina cu task-uri non-preemptive, bare-metal secvent,
ial. Folosit,
i struc
turi de context pentru fiecare task s,
i tablouri cu recurent,
ă, offset s,
i pointer la funct,
ie.
Demonstrat,
i planificarea eficientă a CPU cu un singur task activ per tick