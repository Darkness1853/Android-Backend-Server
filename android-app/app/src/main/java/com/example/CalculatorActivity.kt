package com.example

import android.content.Intent
import android.graphics.Color
import android.os.Bundle
import android.widget.Button
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import com.example.calculator.R
import kotlin.random.Random

class CalculatorActivity : AppCompatActivity() {
    private var display: TextView? = null
    private var currentNumber =""
    private var firstNumber = 0.0
    private var operator = ""
    private var resetDisplay = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_calculator)

        findViewById<Button>(R.id.GoMain).setOnClickListener({
            val Main_Intent = Intent(this, MainActivity::class.java)
            startActivity(Main_Intent)
        });

        display = findViewById(R.id.Display)
        setupNumberButtons()
        setupOperatorButtons()
        setupOtherButtons()

    }
    private fun RandomColor(button: Button) {
        val red = Random.Default.nextInt(256)
        val green = Random.Default.nextInt(256)
        val blue = Random.Default.nextInt(256)
        val color = Color.rgb(red,green, blue)
        button.setBackgroundColor(color)
        button.postDelayed({
            button.setBackgroundColor(Color.parseColor("#5F6ECA"))
        },250)
    }
    private fun RandomColorForOperator(button: Button) {
        val red = Random.Default.nextInt(256)
        val green = Random.Default.nextInt(256)
        val blue = Random.Default.nextInt(256)
        val color = Color.rgb(red,green, blue)
        button.setBackgroundColor(color)
        button.postDelayed({
            button.setBackgroundColor(Color.parseColor("#664FA3FF"))
        },250)
    }
    private fun RandomColorForClear(button: Button) {
        val red = Random.Default.nextInt(256)
        val green = Random.Default.nextInt(256)
        val blue = Random.Default.nextInt(256)
        val color = Color.rgb(red,green, blue)
        button.setBackgroundColor(color)
        button.postDelayed({
            button.setBackgroundColor(Color.parseColor("#A62626"))
        },250)
    }

    private fun setupOperatorButtons(){
        val buttonAdd = findViewById<Button>(R.id.btnAdd)
        buttonAdd.setOnClickListener {
            setOperator("+")
            RandomColorForOperator(buttonAdd)
        }

        val buttonSubstract = findViewById<Button>(R.id.btnSubtract)
        buttonSubstract.setOnClickListener {
            setOperator("-")
            RandomColorForOperator(buttonSubstract)
        }

        val buttonMultiply = findViewById<Button>(R.id.btnMultiply)
        buttonMultiply.setOnClickListener {
            setOperator("*")
            RandomColorForOperator(buttonMultiply)
        }

        val buttonDivide = findViewById<Button>(R.id.btnDivide)
        buttonDivide.setOnClickListener {
            setOperator("/")
            RandomColorForOperator(buttonDivide)
        }

    }

    private fun setupOtherButtons(){
        val buttonEquals = findViewById<Button>(R.id.btnEquals)
        buttonEquals.setOnClickListener {
            calculate()
            RandomColorForOperator(buttonEquals)
        }

        val buttonClear = findViewById<Button>(R.id.btnClear)
        buttonClear.setOnClickListener {
            clearAll()
            RandomColorForClear(buttonClear)
        }

    }
    private fun setupNumberButtons(){
        val button0 = findViewById<Button>(R.id.btn0)
        button0.setOnClickListener {
            addNumber("0")
            RandomColor(button0)
        }

        val button1 = findViewById<Button>(R.id.btn1)
        button1.setOnClickListener {
            addNumber("1")
            RandomColor(button1)
        }

        val button2 = findViewById<Button>(R.id.btn2)
        button2.setOnClickListener {
            addNumber("2")
            RandomColor(button2)
        }

        val button3 = findViewById<Button>(R.id.btn3)
        button3.setOnClickListener {
            addNumber("3")
            RandomColor(button3)
        }

        val button4 = findViewById<Button>(R.id.btn4)
        button4.setOnClickListener {
            addNumber("4")
            RandomColor(button4)
        }

        val button5 = findViewById<Button>(R.id.btn5)
        button5.setOnClickListener {
            addNumber("5")
            RandomColor(button5)
        }

        val button6 = findViewById<Button>(R.id.btn6)
        button6.setOnClickListener {
            addNumber("6")
            RandomColor(button6)
        }

        val button7 = findViewById<Button>(R.id.btn7)
        button7.setOnClickListener {
            addNumber("7")
            RandomColor(button7)
        }

        val button8 = findViewById<Button>(R.id.btn8)
        button8.setOnClickListener {
            addNumber("8")
            RandomColor(button8)
        }

        val button9 = findViewById<Button>(R.id.btn9)
        button9.setOnClickListener {
            addNumber("9")
            RandomColor(button9)
        }

    }

    private fun addNumber(number: String){
        if (resetDisplay){
            currentNumber = ""
            resetDisplay = false
        }

        if (currentNumber == "0") {
            currentNumber = number
        } else {
            currentNumber += number
        }

        display?.text = currentNumber
    }


    private fun setOperator(newOperator: String){
        if (currentNumber != ""){
            firstNumber = currentNumber.toDouble()
            operator = newOperator
            resetDisplay= true
        }

    }

    private fun calculate() {
        if (currentNumber != "" && operator != "") {
            val secondNumber = currentNumber.toDouble()
            var result = 0.0

            if (operator == "+") {
                result = firstNumber + secondNumber
            } else if (operator == "-") {
                result = firstNumber - secondNumber
            } else if (operator == "*") {
                result = firstNumber * secondNumber
            } else {
                if (secondNumber != 0.0) {
                    result = firstNumber / secondNumber
                } else {
                    display?.text = "Error"
                    return
                }
            }
            currentNumber = if (result % 1 == 0.0) {
                result.toInt().toString()
            } else {
                result.toString()
            }
            display?.text = currentNumber
            operator = ""
            resetDisplay = true
        }
    }

    private fun clearAll() {
        currentNumber = ""
        firstNumber = 0.0
        operator = ""
        resetDisplay = false
        display?.text = "0"

    }
}