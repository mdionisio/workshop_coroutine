
###

Abstract:

Ogni programmatore ha imparato ad affrontare i problemi con metodologie top-down o bottom-up, generalmente affrontando la programmazione come processo di scomposizione di problemi in problemi più semplici, più piccolì.

Questa sessione ha lo scopo di insinuare il dubbio se questa sia veramente la strada più naturale per la mente. 

Affronteremo quindi la programmazione cooperativa, per risolvere pattern standard, non tramite un approccio a thread ma estendendo il concetto di sub-routine in coroutine.

#Ogni programmatore ha scritto e avuto a che fare con funzioni e procedure, più in generale con sub-routine. In questo workshop affronteremo le co-routine per evidenziare le potenzialità di una programmazione cooperativa.



###

Indice:

1. problema produttore consumatore.
   1. generatori e costrutto yield in python. Uso di next e send (http://www.dabeaz.com/coroutines/)
   2. problema produttore consumatore risolto con macchine a stati (codice di Simon Tatham, http://www.chiark.greenend.org.uk/~sgtatham/coroutines.html) (in C)
   3. stesso problema con i cambi di contesto (in C e debugger in mano)
   
2. sguardo rapido a setcontext e getcontext nella glibc (in assembly)
   
3. collaborazione con uno scheduler (in C, abbandoniamo il debugger in favore dei log)

4. collaborazione con una programmazione a eventi (libevent) e concetto di greenlet (in C)

###

Cosa Serve

linux   (verifico con MacOSX)
gcc     (verifico con clang)
gdb     (io userò un ide, non mi voglio complicare troppo la vita)
cmake   
un minimo di esperienza di assembly e su come funzionano le chiamate a funzione.

