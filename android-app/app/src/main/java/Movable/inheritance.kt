package Movable

import kotlin.random.Random
import kotlin.concurrent.thread

open class Human {
    var name: String = ""
    var surname: String = ""
    var age: Int = 0
    var x = 0
    var y = 0

    constructor(_name: String, _surname: String, _age: Int) {
        name = _name
        surname = _surname
        age = _age
        println("Создан человек: $name")
    }

    open fun move() {
        x += Random.nextInt(-1, 2)
        y += Random.nextInt(-1, 2)
        println("$name переместился в ($x, $y)")
    }
}

class Driver(_name: String, _surname: String, _age: Int) : Human(_name, _surname, _age) {
    var carModel: String = "Toyota"
    
    init {
        println("Создан водитель: $name")
    }
    
    override fun move() {
        x += 2 
        println("Водитель $name проехал на $carModel в ($x, $y)")
    }
}

fun main() {
    val human1 = Human("Анна", "Иванова", 20)
    val human2 = Human("Иван", "Петров", 25)
    val human3 = Human("Мария", "Сидорова", 22)
    val driver = Driver("Алексей", "Кузнецов", 30)
    
    println("Сколько секунд двигаться?")
    val time = readLine()?.toIntOrNull() ?: 0
    if (time <= 0) return
    println("Начинаем движение!")
    for (second in 1..time) {
        println("\nСекунда $second")
        val thread1 = thread { human1.move() }
        val thread2 = thread { human2.move() }
        val thread3 = thread { human3.move() }
        val thread4 = thread { driver.move() } 
        thread1.join()
        thread2.join()
        thread3.join()
        thread4.join()
    }
    println("\nРезультаты")
    println("${human1.name}: (${human1.x}, ${human1.y})")
    println("${human2.name}: (${human2.x}, ${human2.y})")
    println("${human3.name}: (${human3.x}, ${human3.y})")
    println("${driver.name}: (${driver.x}, ${driver.y})")
}
