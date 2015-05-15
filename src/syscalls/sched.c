#include <sched.h>
#include <stdio.h>

int main(){
    struct sched_param param, new_param;
 
   /*
    * Процесс запускается с политикой по умолчанию SCHED_OTHER,
    * если не порождён процессом SCHED_RR или SCHED_FIFO.
    */

    printf("start policy = %d\n", sched_getscheduler(0));
    /*
    * выводит -> start policy = 0 .
    * (Для политик SCHED_FIFO или SCHED_RR, sched_getscheduler
    * возвращает 1 и 2 соответственно)
    */

    /*
    * Создаём процесс SCHED_FIFO, работающий со средним приоритетом
    */
    param.sched_priority = (sched_get_priority_min(SCHED_FIFO) +
                             sched_get_priority_max(SCHED_FIFO))/2;
    printf("max priority = %d, min priority = %d, my priority = %d\n",
                                   sched_get_priority_max(SCHED_FIFO),
                                   sched_get_priority_min(SCHED_FIFO),
                                                param.sched_priority);
    /*
    * выводит -> max priority = 99, min priority = 1,
    * my priority = 50
    */

    /* Делаем процесс SCHED_FIFO */
    if (sched_setscheduler(0, SCHED_FIFO, &param) != 0){
        perror("sched_setscheduler failed\n");
        return;
    }

    /*
    * выполнение критичных ко времени операций
    */

    /*
    * Даём шанс поработать какому-либо другому потоку/процессу
    * реального времени. Обратите внимание, что вызов sched_yield
    * поместит текущий процесс в конец очереди с его приоритетом.
    * Если в этой очереди нет другого процесса, этот вызов
    * не будет иметь эффекта
    */
    sched_yield();

    /* Вы можете также изменять приоритет во время работы */
    param.sched_priority = sched_get_priority_max(SCHED_FIFO);
    if (sched_setparam(0, &param) != 0){
        perror("sched_setparam failed\n");
        return;
    }
    sched_getparam(0, &new_param);
    printf("I am running at priority %d\n",
                                   new_param.sched_priority);
    /* выводит -> I am running at priority 99 */
    return ;
}
