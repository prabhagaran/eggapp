package com.eggapp.field.push

import android.app.NotificationChannel
import android.app.NotificationManager
import android.app.PendingIntent
import android.content.Context
import android.content.Intent
import android.os.Build
import androidx.core.app.NotificationCompat
import androidx.core.app.NotificationManagerCompat
import com.eggapp.field.MainActivity
import com.eggapp.field.R
import com.eggapp.field.data.ApiClient
import com.eggapp.field.data.PushTokenRequest
import com.eggapp.field.data.TokenStore
import com.google.firebase.messaging.FirebaseMessagingService
import com.google.firebase.messaging.RemoteMessage
import kotlinx.coroutines.CoroutineScope
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch

/**
 * Receives pushes (US-NOT-002) and registers this device's token with the
 * backend. Registration also happens right after login (see LoginViewModel)
 * since onNewToken only fires on rotation, not on every app start.
 */
class EggAppMessagingService : FirebaseMessagingService() {

    override fun onNewToken(token: String) {
        val tokenStore = TokenStore(applicationContext)
        if (tokenStore.accessToken() == null) return // not logged in yet; login flow will register it
        registerToken(applicationContext, token)
    }

    override fun onMessageReceived(message: RemoteMessage) {
        val title = message.notification?.title ?: "eggAPP alert"
        val body = message.notification?.body ?: ""
        showNotification(applicationContext, title, body)
    }

    companion object {
        private const val CHANNEL_ID = "eggapp_alerts"
        private var nextNotificationId = 1000

        fun registerToken(context: Context, token: String) {
            CoroutineScope(Dispatchers.IO).launch {
                runCatching {
                    ApiClient.authenticated(TokenStore(context)).registerPushToken(PushTokenRequest(token))
                }
            }
        }

        fun showNotification(context: Context, title: String, body: String) {
            val manager = context.getSystemService(Context.NOTIFICATION_SERVICE) as NotificationManager
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                manager.createNotificationChannel(
                    NotificationChannel(CHANNEL_ID, "Environmental alerts", NotificationManager.IMPORTANCE_HIGH),
                )
            }

            val intent = Intent(context, MainActivity::class.java).apply {
                flags = Intent.FLAG_ACTIVITY_NEW_TASK or Intent.FLAG_ACTIVITY_CLEAR_TASK
            }
            val pendingIntent = PendingIntent.getActivity(
                context, 0, intent,
                PendingIntent.FLAG_IMMUTABLE or PendingIntent.FLAG_UPDATE_CURRENT,
            )

            val notification = NotificationCompat.Builder(context, CHANNEL_ID)
                .setSmallIcon(R.drawable.ic_notification)
                .setContentTitle(title)
                .setContentText(body)
                .setPriority(NotificationCompat.PRIORITY_HIGH)
                .setAutoCancel(true)
                .setContentIntent(pendingIntent)
                .build()

            NotificationManagerCompat.from(context).notify(nextNotificationId++, notification)
        }
    }
}
