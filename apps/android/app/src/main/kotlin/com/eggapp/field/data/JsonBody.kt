package com.eggapp.field.data

import okhttp3.MediaType.Companion.toMediaType
import okhttp3.RequestBody
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject

private val JSON_MEDIA_TYPE = "application/json; charset=utf-8".toMediaType()

// Gson's default converter silently omits null fields — fine for "leave
// unchanged", but the backend distinguishes "absent" (unchanged) from
// explicit JSON `null` (clear) for a couple of optional fields
// (Incubator.defaultSpeciesId, Flock.stageOverride). CLEAR forces that
// field to serialize as null; a plain Kotlin null omits the key entirely.
object CLEAR

/** Builds a PATCH body by hand so [CLEAR] can force a real JSON null through. */
fun jsonPatchBody(vararg fields: Pair<String, Any?>): RequestBody {
    val json = JSONObject()
    for ((key, value) in fields) {
        when (value) {
            null -> {}
            CLEAR -> json.put(key, JSONObject.NULL)
            else -> json.put(key, value)
        }
    }
    return json.toString().toRequestBody(JSON_MEDIA_TYPE)
}
