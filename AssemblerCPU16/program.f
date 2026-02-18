// Глобальные переменные
int results[5];
int size = 5;

// Рекурсивная функция факториала
// Проверяет: передачу параметров, сохранение BP, рекурсию и RET
int factorial(int n) {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

// Функция для инициализации массива через указатель
// Проверяет: работу с указателями и аргументами


int main() {
    int i;
    int total_sum = 0;
    int fact_res;

    // 1. Вычисляем факториал 5 (должно быть 120)
    fact_res = factorial(5);

    // 2. Инициализируем глобальный массив
    // Передаем адрес массива results
    //init_array(results, size);

    // 3. Считаем сумму элементов массива и сохраняем результаты
    for (i = 0; i < size; i++) {
        int val = results[i];
        total_sum = total_sum + val;
    }

    // Итоговый результат: 120 + (1+2+3+4+5) = 120 + 15 = 135
    return total_sum + fact_res;
}