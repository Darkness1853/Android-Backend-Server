package com.example

import android.os.Bundle
import android.widget.Button
import android.content.Intent
import androidx.appcompat.app.AppCompatActivity
import com.example.calculator.R

class MainActivity : AppCompatActivity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_main)

        findViewById<Button>(R.id.GoCalculator).setOnClickListener({
            val calc_Intent = Intent(this, CalculatorActivity::class.java)
            startActivity(calc_Intent)
        });

        findViewById<Button>(R.id.GoMedia).setOnClickListener({
            val Media_Intent = Intent(this, MediaActivity::class.java)
            startActivity(Media_Intent)
        });

        findViewById<Button>(R.id.GoLocate).setOnClickListener({
            val Locate_Intent = Intent(this, LocateActivity::class.java)
            startActivity(Locate_Intent)
        });

        findViewById<Button>(R.id.GoMobile_connect).setOnClickListener({
            val Mobile_Connect_Intent = Intent(this, Mobile_ConnectActivity::class.java)
            startActivity(Mobile_Connect_Intent)
        });

        findViewById<Button>(R.id.GoSoket).setOnClickListener({
            val Soket_Intent = Intent(this, SoketActivity::class.java)
            startActivity(Soket_Intent)
        });
        findViewById<Button>(R.id.Exit).setOnClickListener ({
            finishAffinity()
        });

    }
}