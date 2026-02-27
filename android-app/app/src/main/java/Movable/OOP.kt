package Movable

import kotlin.random.Random

class Human {
    var name: String = ""
    var surname: String = ""
    var second_name: String = ""
    var age: Int = 0
    var currentSpeed: Double = 0.0

    var group_number: Int = -1
    var x = 0
    var y = 0

    constructor(_name: String, _surname: String, _second: String, _gn: Int, _age: Int) {
        name = _name
        surname = _surname
        second_name = _second
        group_number = _gn
        age = _age
        currentSpeed = 1.0 
        println("Мы создали объект Human с именем: $name")
    }

    fun move() {
        val dx = Random.nextInt(-1, 2)
        val dy = Random.nextInt(-1, 2) 
        
        x += dx
        y += dy
        
        println("$name $surname moved to: ($x, $y)")
    }
    
    fun moveTo(_toX: Int, _toY: Int) {
        x = _toX 
        y = _toY  
        println("$name $surname moved TO: ($x, $y)")  
    }

    fun getCoordinates(): String = "($x, $y)"
}

fun main() {
    val humans = arrayOf(
        Human("Эртине", "Артаа", "Адыгжыевич", 444, 20),
        Human("Арина", "Бубенина", "Ивановна", 444, 21),
        Human("Семён", "Бучельников", "Александрович", 444, 22),
        Human("Дмитрий", "Воробьёв", "Андреевич", 444, 19),
        Human("Егор", "Григорьев", "Васильевич", 444, 20),
        Human("Сергей", "Демин", "Алексеевич", 444, 21),
        Human("Артём", "Добромилов", "Александрович", 444, 22),
        Human("Мария", "Дроздова", "Геннадьевна", 444, 19),
        Human("Андрей", "Золотухин", "Алексеевич", 444, 20),
        Human("Мухаммадамин", "Каршибоев", "Мухбиржонович", 444, 21),
        Human("Максим", "Кит", "Аркадьевич", 444, 22),
        Human("Анна", "Короткова", "Павловна", 444, 19),
        Human("Кирилл", "Крахмальный", "Вячеславович", 444, 20),
        Human("Никита", "Криволапов", "Алексеевич", 444, 21),
        Human("Андрей", "Кутенков", "Алексеевич", 444, 22),
        Human("Матвей", "Лазарев", "Олегович", 444, 19),
        Human("Никита", "Лахтионов", "Сергеевич", 444, 20),
        Human("Валерия", "Лысенко", "Андреевна", 444, 21)
    )

    println("Введите длительность симуляции:")
    val input = readLine()
    var simulationTime: Int = 0 

    if (input != null) {
        try {
            simulationTime = input.toInt()
        } catch (e: NumberFormatException) {
            simulationTime = 0
            println("Ошибка: введите число!")
        }
    } else {
        simulationTime = 0
        println("Не введено значение")
    }
    
    if (simulationTime <= 0) {
        println("Некорректная длительность симуляции")
        return
    }

    println("\nНачальные координаты всех участников: (0, 0)")

    var second = 1
    while (second <= simulationTime) {
        println("${second}-я секунда")
        
        var i = 0
        while (i < humans.size) {
            humans[i].move()
            i++
        }
        
        println()
        second++
    }

    println("\nФинальные координаты:")
    for (human in humans) {
        println("${human.surname} ${human.name} ${human.second_name}: ${human.getCoordinates()}")
    }
}
