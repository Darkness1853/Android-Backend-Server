package com.example

import android.content.Intent
import android.os.Bundle
import android.util.Log
import android.widget.Button
import android.widget.EditText
import android.widget.TextView
import androidx.activity.enableEdgeToEdge
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowInsetsCompat
import com.example.calculator.R
import org.zeromq.SocketType
import org.zeromq.ZMQ
import org.zeromq.ZContext

class SoketActivity : AppCompatActivity() {
    private var log_tag: String = "MY_LOG_TAG"
    private lateinit var Status: TextView
    private lateinit var Response: TextView
    private var messageCount = 0
    private lateinit var btnSend: Button
    private lateinit var btnTestConnect: Button
    private lateinit var ServerIp: EditText
    private lateinit var Serverport: EditText

    private lateinit var ResponseServer: TextView
    private var Connect = false

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        enableEdgeToEdge()
        setContentView(R.layout.activity_soket)

        ViewCompat.setOnApplyWindowInsetsListener(findViewById(R.id.main)) { v, insets ->
            val systemBars = insets.getInsets(WindowInsetsCompat.Type.systemBars())
            v.setPadding(systemBars.left, systemBars.top, systemBars.right, systemBars.bottom)
            insets
        }

        Status = findViewById(R.id.Status)
        Response = findViewById(R.id.Response)
        btnSend = findViewById(R.id.bthSend1)
        btnTestConnect = findViewById(R.id.btnTestConnect)
        ServerIp = findViewById(R.id.ServerIp)
        Serverport = findViewById(R.id.ServerPort)
        ResponseServer = findViewById(R.id.ResponseServer)

        updateConnectionStatus(false, "Не подключено")

        btnSend.setOnClickListener {
            if (Connect) {
                sendMessageToServer()
            } else {
                android.widget.Toast.makeText(
                    this, "Проверьте подключение сервера", android.widget.Toast.LENGTH_SHORT
                ).show()
            }
        }

        btnTestConnect.setOnClickListener {
            testConnection()
        }

        findViewById<Button>(R.id.GoMain).setOnClickListener {
            val mainIntent = Intent(this, MainActivity::class.java)
            startActivity(mainIntent)
        }
    }

    private fun testConnection() {
            try {
                messageCount=0
                updateConnectionStatus(false, "Проверка подключения")
                Thread.sleep(1000)
                val serverIp = ServerIp.text.toString()
                val serverPort = Serverport.text.toString()

                if (serverIp.isEmpty() || serverPort.isEmpty()) {
                    updateConnectionStatus(false, "Введите IP и порт")
                }

                val context = ZContext()
                val socket = context.createSocket(SocketType.REQ)
                socket.setReceiveTimeOut(2000)
                socket.setSendTimeOut(2000)

                val address = "tcp://$serverIp:$serverPort"
                Log.d(log_tag, "Попытка подключения к: $address")

                socket.connect(address)
                val testMessage = "TEST_CONNECT"
                socket.send(testMessage.toByteArray(ZMQ.CHARSET), 0)

                val reply = socket.recv(0)
                if (reply != null) {
                    val responseText = String(reply, ZMQ.CHARSET)
                    Log.d(log_tag, "Тестовый ответ: $responseText")

                    Connect = true
                    updateConnectionStatus(true, "Подключено к $serverIp:$serverPort")
                    android.widget.Toast.makeText(this, "Подключение успешно!", android.widget.Toast.LENGTH_SHORT).show()
                }

                socket.close()
                context.close()

            } catch (e: Exception) {
                Log.e(log_tag, "Ошибка подключения: ${e.message}")
                Connect = false
                updateConnectionStatus(false, "Ошибка подключения: ${e.message}")
                android.widget.Toast.makeText(this, "Не удалось подключиться: ${e.message}", android.widget.Toast.LENGTH_SHORT).show()
            }
    }

    private fun sendMessageToServer() {
            try {
                messageCount++

                val serverIp = ServerIp.text.toString()
                val serverPort = Serverport.text.toString()

                val context = ZContext()
                val socket = context.createSocket(SocketType.REQ)
                socket.connect("tcp://$serverIp:$serverPort")
                socket.setReceiveTimeOut(5000)
                socket.setSendTimeOut(5000)

                val request = "Hello Server Message #$messageCount"
                Response.text = "Отправка: $request"

                Log.d(log_tag, "Отправка: $request")
                socket.send(request.toByteArray(ZMQ.CHARSET), 0)

                val reply = socket.recv(0)
                val responseText = String(reply, ZMQ.CHARSET)

                Log.d(log_tag, "Получено: $responseText")
                ResponseServer.text = "Получено: $responseText"

                socket.close()
                context.close()

            } catch (e: Exception) {
                Log.e(log_tag, "Ошибка: ${e.message}")

                    Connect = false
                    updateConnectionStatus(false, "Ошибка отправки")
                    Response.text = "Ошибка: ${e.message}"
                    android.widget.Toast.makeText(this, "Ошибка отправки: ${e.message}", android.widget.Toast.LENGTH_SHORT)
            }
    }

    private fun updateConnectionStatus(connected: Boolean, message: String) {
            if (connected) {
                Status.text = "Подключено: $message"
                btnSend.isEnabled = true
            } else {
                Status.text = "Не подключено: $message"
                btnSend.isEnabled = false
        }
    }
}