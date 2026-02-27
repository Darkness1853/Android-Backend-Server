package com.example

import android.Manifest
import android.content.ContentUris
import android.content.Intent
import android.content.pm.PackageManager
import android.media.AudioAttributes
import android.media.AudioManager
import android.media.MediaPlayer
import android.net.Uri
import android.os.*
import android.provider.MediaStore
import android.widget.*
import androidx.activity.result.contract.ActivityResultContracts
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import com.example.calculator.R

data class AudioTrack(
    val id: Long,
    val title: String,
    val artist: String,
    val duration: Long,
    val uri: Uri,
    val path: String
)

class MediaActivity : AppCompatActivity() {

    private lateinit var mediaPlayer: MediaPlayer
    private lateinit var audioManager: AudioManager
    private val handler = Handler(Looper.getMainLooper())
    private var isPlaying = false
    private var currentTrackIndex = -1

    private val audioTracks = mutableListOf<AudioTrack>()
    private lateinit var spinnerTracks: Spinner
    private lateinit var spinnerAdapter: ArrayAdapter<String>

    private lateinit var buttonPlayStop: Button
    private lateinit var buttonPrev: Button
    private lateinit var buttonNext: Button
    private lateinit var buttonExit: Button
    private lateinit var seekBarProgress: SeekBar
    private lateinit var seekBarVolume: SeekBar
    private lateinit var textViewCurrentTrack: TextView
    private lateinit var textway: TextView
    private lateinit var textViewCurrentTime: TextView
    private lateinit var textViewTotalTime: TextView

    private val requestPermissionLauncher = registerForActivityResult(
        ActivityResultContracts.RequestPermission()
    ) { isGranted ->
        if (isGranted) {
            loadAudioTracks()
        } else {
            Toast.makeText(this, "Разрешение необходимо для доступа к музыке", Toast.LENGTH_LONG).show()
        }
    }

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        setContentView(R.layout.activity_media)

        initViews()
        initMediaPlayer()
        checkPermissions()
        setupVolumeControl()
    }

    private fun initViews() {
        spinnerTracks = findViewById(R.id.spinnerTracks)
        buttonPlayStop = findViewById(R.id.buttonPlayStop)
        buttonPrev = findViewById(R.id.buttonPrev)
        buttonNext = findViewById(R.id.buttonNext)
        buttonExit = findViewById(R.id.buttonExit)
        seekBarProgress = findViewById(R.id.seekBarProgress)
        seekBarVolume = findViewById(R.id.seekBarVolume)
        textViewCurrentTrack = findViewById(R.id.textViewCurrentTrack)
        textViewCurrentTime = findViewById(R.id.textViewCurrentTime)
        textViewTotalTime = findViewById(R.id.textViewTotalTime)
        textway = findViewById(R.id.Way)

        spinnerAdapter = ArrayAdapter(this, android.R.layout.simple_spinner_item)
        spinnerAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item)
        spinnerTracks.adapter = spinnerAdapter

        spinnerTracks.onItemSelectedListener = object : AdapterView.OnItemSelectedListener {
            override fun onItemSelected(parent: AdapterView<*>?, view: android.view.View?, position: Int, id: Long) {
                if (position >= 0 && position < audioTracks.size) {
                    playTrack(position)
                }
            }

            override fun onNothingSelected(parent: AdapterView<*>?) {
            }
        }

        buttonPlayStop.setOnClickListener {
            togglePlayStop()
        }

        buttonPrev.setOnClickListener {
            playPreviousTrack()
        }

        buttonNext.setOnClickListener {
            playNextTrack()
        }

        buttonExit.setOnClickListener {
            val intent = Intent(this, MainActivity::class.java)
            startActivity(intent)
            finish()
        }

        seekBarProgress.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    mediaPlayer.seekTo(progress)
                }
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })
    }

    private fun initMediaPlayer() {
        mediaPlayer = MediaPlayer().apply {
            setAudioAttributes(
                AudioAttributes.Builder()
                    .setContentType(AudioAttributes.CONTENT_TYPE_MUSIC)
                    .setUsage(AudioAttributes.USAGE_MEDIA)
                    .build()
            )
            setOnCompletionListener {
                playNextTrack()
            }
            setOnPreparedListener {
                updateTrackInfo()
                startPlayback()
            }
        }
    }

    private fun checkPermissions() {
        val permission = if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.TIRAMISU) {
            Manifest.permission.READ_MEDIA_AUDIO
        } else {
            Manifest.permission.READ_EXTERNAL_STORAGE
        }

        if (ContextCompat.checkSelfPermission(this, permission) == PackageManager.PERMISSION_GRANTED) {
            loadAudioTracks()
        } else {
            requestPermissionLauncher.launch(permission)
        }
    }

    private fun loadAudioTracks() {
        val collection = MediaStore.Audio.Media.EXTERNAL_CONTENT_URI

        val projection = arrayOf(
            MediaStore.Audio.Media._ID,
            MediaStore.Audio.Media.TITLE,
            MediaStore.Audio.Media.ARTIST,
            MediaStore.Audio.Media.DURATION,
            MediaStore.Audio.Media.DATA,
        )

        val selection = "${MediaStore.Audio.Media.IS_MUSIC} != 0"

        contentResolver.query(
            collection,
            projection,
            selection,
            null,
            MediaStore.Audio.Media.TITLE
        )?.use { cursor ->
            val idColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media._ID)
            val titleColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.TITLE)
            val artistColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.ARTIST)
            val durationColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DURATION)
            val pathColumn = cursor.getColumnIndexOrThrow(MediaStore.Audio.Media.DATA)

            audioTracks.clear()
            spinnerAdapter.clear()

            while (cursor.moveToNext()) {
                val id = cursor.getLong(idColumn)
                val title = cursor.getString(titleColumn) ?: "Неизвестный трек"
                val artist = cursor.getString(artistColumn) ?: "Неизвестный исполнитель"
                val duration = cursor.getLong(durationColumn)
                val uri = ContentUris.withAppendedId(MediaStore.Audio.Media.EXTERNAL_CONTENT_URI, id)
                val path = cursor.getString(pathColumn)
                val track = AudioTrack(id, title, artist, duration, uri,path)

                audioTracks.add(track)
                spinnerAdapter.add("$title - $artist")
            }

            if (audioTracks.isNotEmpty()) {
                spinnerAdapter.notifyDataSetChanged()
                Toast.makeText(this, "Загружено ${audioTracks.size} треков", Toast.LENGTH_SHORT).show()
            } else {
                Toast.makeText(this, "Аудиофайлы не найдены", Toast.LENGTH_SHORT).show()
            }
        }
    }

    private fun playTrack(position: Int) {
        if (position < 0 || position >= audioTracks.size) return

        currentTrackIndex = position
        val track = audioTracks[position]

        textway.text= track.path

        mediaPlayer.reset()
        try {
            mediaPlayer.setDataSource(applicationContext, track.uri)
            mediaPlayer.prepareAsync()
        } catch (e: Exception) {
            Toast.makeText(this, "Ошибка загрузки трека", Toast.LENGTH_SHORT).show()
        }
    }

    private fun togglePlayStop() {
        if (!mediaPlayer.isPlaying) {
            if (currentTrackIndex == -1 && audioTracks.isNotEmpty()) {
                spinnerTracks.setSelection(0)
                playTrack(0)
            } else {
                startPlayback()
            }
        } else {
            stopPlayback()
        }
    }

    private fun startPlayback() {
        try {
            mediaPlayer.start()
            isPlaying = true
            buttonPlayStop.setBackgroundResource(android.R.drawable.ic_media_pause)
            startProgressUpdates()
        } catch (e: IllegalStateException) {
            Toast.makeText(this, "Ошибка воспроизведения", Toast.LENGTH_SHORT).show()
        }
    }

    private fun stopPlayback() {
        mediaPlayer.pause()
        isPlaying = false
        buttonPlayStop.setBackgroundResource(android.R.drawable.ic_media_play)
        stopProgressUpdates()
    }

    private fun playPreviousTrack() {
        if (audioTracks.isEmpty()) return
        val newIndex = if (currentTrackIndex > 0) currentTrackIndex - 1 else audioTracks.size - 1
        spinnerTracks.setSelection(newIndex)
        playTrack(newIndex)
    }

    private fun playNextTrack() {
        if (audioTracks.isEmpty()) return
        val newIndex = if (currentTrackIndex < audioTracks.size - 1) currentTrackIndex + 1 else 0
        spinnerTracks.setSelection(newIndex)
        playTrack(newIndex)

    }

    private fun updateTrackInfo() {
        if (currentTrackIndex >= 0 && currentTrackIndex < audioTracks.size) {
            val track = audioTracks[currentTrackIndex]
            textViewCurrentTrack.text = "${track.title} - ${track.artist}"
            textViewTotalTime.text = formatTime(track.duration)
            textway.text= track.path
            seekBarProgress.max = track.duration.toInt()
            seekBarProgress.progress = 0
        }
    }

    private fun startProgressUpdates() {
        handler.post(object : Runnable {
            override fun run() {
                if (mediaPlayer.isPlaying) {
                    val currentPos = mediaPlayer.currentPosition
                    val duration = mediaPlayer.duration

                    seekBarProgress.max = duration
                    seekBarProgress.progress = currentPos

                    textViewCurrentTime.text = formatTime(currentPos.toLong())
                    textViewTotalTime.text = formatTime(duration.toLong())

                    handler.postDelayed(this, 1000)
                }
            }
        })
    }

    private fun stopProgressUpdates() {
        handler.removeCallbacksAndMessages(null)
    }

    private fun setupVolumeControl() {
        audioManager = getSystemService(AUDIO_SERVICE) as AudioManager

        seekBarVolume.setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
            override fun onProgressChanged(seekBar: SeekBar?, progress: Int, fromUser: Boolean) {
                if (fromUser) {
                    val volume = progress / 100.0f
                    mediaPlayer.setVolume(volume, volume)
                }
            }
            override fun onStartTrackingTouch(seekBar: SeekBar?) {}
            override fun onStopTrackingTouch(seekBar: SeekBar?) {}
        })
    }

    private fun formatTime(millis: Long): String {
        val totalSeconds = millis / 1000
        val minutes = totalSeconds / 60
        val seconds = totalSeconds % 60
        return String.format("%02d:%02d", minutes, seconds)
    }

    override fun onPause() {
        super.onPause()
    }

    override fun onResume() {
        super.onResume()
    }

    override fun onDestroy() {
        super.onDestroy()
        stopProgressUpdates()
        mediaPlayer.release()
    }
}