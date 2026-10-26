# Specificații Tehnice - Remote Administration Tool
**Proiectarea Sistemelor de Operare - Proiect**

---

## 1. Descrierea Proiectului
  Remote Administration Tool:
  
    - Server centralizator
    
    - Mai multi agenti care coleacteaza date de pe calculatoare
    
    - Poate executa comenzi pe mai multe calculatoare simultan
    
    - Screenshot-uri/live stream
    
    - Alerte accesare anumite site-uri

## Contribuitori
  - Jitaru Tudor
  - Voiculescu Mihai

---

## 2. Cerințe

Sistemul este alcătuit din două componente principale:
1. **Server Centralizator:** Gestionează agenții conectați, primește datele de telemetrie, trimite comenzi selective sau de tip broadcast și monitorizează alertele.

2. **Agent de Monitorizare:** Serviciu care rulează pe mașinile țintă: colectează informații de sistem, execută comenzi la cerere, realizează capturi de ecran/stream și monitorizează traficul web.

---

### 2.1. Cerințe Funcționale 

  **CFN1.1** Conectivitate Multi-Agent: Serverul trebuie să accepte conexiuni simultane de la mai mulți agenți.

  **CFN1.2** Mecanism Heartbeat: Agenții vor trimite un semnal periodic pentru a confirma starea activă.

  **CFN2.1** Colectare Telemetrie: Agentul va extrage utilizarea CPU, RAM, disc și lista proceselor active.

  **CFN2.2** Transmitere Informații: Informațiile vor fi transmise periodic către server și afișate în panoul de control.

  **CFN3.1** Execuție Distributivă: Serverul va permite trimiterea unei comenzi shell către un agent specific sau către un grup de agenți.

  **CFN3.2** Agregare Rezultate: Ieșirea comenzii (`stdout` / `stderr`) va fi capturată de agent și returnată serverului.

  **CFN4.1** Screenshot la Cerere: La comandă, agentul capturează ecranul curent(`scrot` sau `flameshot`) și îl trimite ca imagine comprimată.

  **CFN4.2** Stream Video: Permite flux de cadre pentru monitorizarea vizuală a mașinii client.

  **CFN5.1** Filtrare/Detectare Acces Web: Agentul va monitoriza conexiunile active / cererile DNS către domenii aflate pe o listă neagră.

  **CFN5.2** Sistem de Alerte: La detectarea unui site restricționat, agentul trimite un pachet de alertă cu prioritate mare.

---

### 2.2. Cerințe Nefuncționale

* **CN1 - Performanță și Concurență:** Serverul va utiliza multiplexare I/O non-blocantă (`epoll`) sau un model bazat pe thread-uri.
* **CN2 - Impact Minim pe Agent:** Agentul va fi optimizat pentru a consuma resurse reduse.
* **CN3 - Reconectare Automată:** În cazul pierderii conexiunii cu serverul, agentul va încerca reconectarea automată la un interval configurabil.

---
