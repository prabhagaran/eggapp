package com.eggapp.field.data

import android.content.Context
import com.google.gson.Gson

/**
 * Caches the last successfully-fetched Batch per id, so the offline
 * recording forms (candling/hatch) stay available when the live fetch
 * fails — which is exactly the situation an offline-first field app
 * needs to handle, not the exception. A worker syncs once at the start
 * of the day (batch status, schedule), then goes fully offline in the
 * barn; the form must still show up from the cached state.
 *
 * BR-002/BR-008 (only incubating batches accept candling, etc.) are
 * enforced authoritatively server-side regardless — this cache only
 * affects what the client optimistically shows, never what the server
 * accepts.
 */
class BatchCache(context: Context) {
    private val prefs = context.applicationContext.getSharedPreferences("eggapp_batch_cache", Context.MODE_PRIVATE)
    private val gson = Gson()

    fun save(batch: Batch) {
        prefs.edit().putString(batch.id, gson.toJson(batch)).apply()
    }

    fun get(batchId: String): Batch? =
        prefs.getString(batchId, null)?.let { runCatching { gson.fromJson(it, Batch::class.java) }.getOrNull() }
}
