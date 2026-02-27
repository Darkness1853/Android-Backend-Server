package com.example

import android.Manifest
import android.content.Intent
import android.content.pm.PackageManager
import android.location.Location
import android.location.LocationListener
import android.location.LocationManager
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.widget.Button
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.app.ActivityCompat
import androidx.core.content.ContextCompat
import org.json.JSONObject
import java.io.File
import java.io.FileWriter
import java.text.SimpleDateFormat
import java.util.*
import com.example.calculator.R

class LocateActivity : AppCompatActivity(), LocationListener {

    private lateinit var tvLat: TextView
    private lateinit var tvLon: TextView
    private lateinit var tvAlt: TextView
    private lateinit var tvTime: TextView
    private lateinit var locationManager: LocationManager

    private var lastLocation: Location? = null
    private val handler = Handler(Looper.getMainLooper())

    private val timeForm = SimpleDateFormat("dd.MM.yyyy HH:mm:ss", Locale.getDefault())

    companion object {
        private const val PERMISSION_REQUEST_CODE = 100
        private const val JSON_FILENAME = "location.json"
    }

    private val update = object : Runnable {
        override fun run() {
            updateTime()
            handler.postDelayed(this, 1000)
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_locate)

        tvLat = findViewById(R.id.tv_lat)
        tvLon = findViewById(R.id.tv_lon)
        tvAlt = findViewById(R.id.tv_alt)
        tvTime = findViewById(R.id.tv_time)

        locationManager = getSystemService(LOCATION_SERVICE) as LocationManager

        val btnSave = findViewById<Button>(R.id.btnSave1)
        btnSave.setOnClickListener {
            SaveJson()
        }

        val btnExit = findViewById<Button>(R.id.GoMain)
        btnExit.setOnClickListener {
            val mainIntent = Intent(this, MainActivity::class.java)
            startActivity(mainIntent)
            finish()
        }
        checkPermissionsAndStartUpdates()
    }

    override fun onResume() {
        super.onResume()
        handler.post(update)

        if (checkPermissions()) {
            startLocationUpdates()
        }
    }

    override fun onPause() {
        super.onPause()
        handler.removeCallbacks(update)
        stopLocationUpdates()
    }

    override fun onDestroy() {
        super.onDestroy()
        handler.removeCallbacks(update)
    }

    private fun updateTime() {
        runOnUiThread {
            val currentTime = timeForm.format(Date())
            tvTime.text = "Время: $currentTime"
        }
    }

    private fun checkPermissionsAndStartUpdates() {
        if (checkPermissions()) {
            startLocationUpdates()
        } else {
            requestPermissions()
        }
    }

    private fun checkPermissions(): Boolean {
        return ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.ACCESS_FINE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED && ContextCompat.checkSelfPermission(
            this,
            Manifest.permission.ACCESS_COARSE_LOCATION
        ) == PackageManager.PERMISSION_GRANTED
    }

    private fun requestPermissions() {
        ActivityCompat.requestPermissions(
            this,
            arrayOf(
                Manifest.permission.ACCESS_FINE_LOCATION,
                Manifest.permission.ACCESS_COARSE_LOCATION
            ),
            PERMISSION_REQUEST_CODE
        )
    }

    private fun startLocationUpdates() {
        getLastKnownLocation()

        if (ActivityCompat.checkSelfPermission(
                this,
                Manifest.permission.ACCESS_FINE_LOCATION
            ) == PackageManager.PERMISSION_GRANTED
        ) {
            try {
                locationManager.requestLocationUpdates(
                    LocationManager.NETWORK_PROVIDER,
                    1000L,
                    1f,
                    this
                )
                locationManager.requestLocationUpdates(
                    LocationManager.GPS_PROVIDER,
                    1000L,
                    1f,
                    this
                )
            } catch (e: Exception) {
                Toast.makeText(this, "Ошибка запуска GPS", Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun stopLocationUpdates() {
        try {
            locationManager.removeUpdates(this)
        } catch (e: Exception) {
        }
    }

    private fun getLastKnownLocation() {
        if (ActivityCompat.checkSelfPermission(
                this,
                Manifest.permission.ACCESS_FINE_LOCATION
            ) != PackageManager.PERMISSION_GRANTED
        ) {
            return
        }

        try {
            val location = locationManager.getLastKnownLocation(LocationManager.GPS_PROVIDER)
            location?.let {
                updateLocationUI(it)
            }
        } catch (e: Exception) {
        }
    }

    override fun onLocationChanged(location: Location) {
        updateLocationUI(location)
        saveToJson(location)
    }

    private fun updateLocationUI(location: Location) {
        runOnUiThread {
            tvLat.text = String.format("Широта: %.6f", location.latitude)
            tvLon.text = String.format("Долгота: %.6f", location.longitude)
            tvAlt.text = String.format("Высота: %.1f м", location.altitude)
        }
        lastLocation = location
    }

    private fun SaveJson() {
        if (lastLocation != null) {
            saveToJson(lastLocation!!)
            Toast.makeText(this, "Файл сохранен", Toast.LENGTH_SHORT).show()
        } else {
            Toast.makeText(this, "Нет данных о местоположении", Toast.LENGTH_SHORT).show()
        }
    }

    private fun saveToJson(location: Location) {
        try {
            val jsonObject = JSONObject()
            jsonObject.put("latitude", location.latitude)
            jsonObject.put("longitude", location.longitude)
            jsonObject.put("altitude", location.altitude)
            jsonObject.put("current_time", timeForm.format(Date()))
            jsonObject.put("timestamp", location.time)

            val file = File(filesDir, JSON_FILENAME)
            FileWriter(file).use { writer ->
                writer.write(jsonObject.toString(2))
            }

        } catch (e: Exception) {
        }
    }

    override fun onRequestPermissionsResult(
        requestCode: Int,
        permissions: Array<String>,
        grantResults: IntArray
    ) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)

        if (requestCode == PERMISSION_REQUEST_CODE) {
            if (grantResults.isNotEmpty() && grantResults[0] == PackageManager.PERMISSION_GRANTED) {
                startLocationUpdates()
                handler.post(update)
            } else {
                Toast.makeText(this, "Разрешения не получены", Toast.LENGTH_LONG).show()
            }
        }
    }
}