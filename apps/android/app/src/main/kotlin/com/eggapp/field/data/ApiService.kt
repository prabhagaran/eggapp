package com.eggapp.field.data

import retrofit2.Response
import retrofit2.http.Body
import retrofit2.http.GET
import retrofit2.http.POST
import retrofit2.http.Path
import retrofit2.http.Query

interface ApiService {
    @POST("v1/auth/login")
    suspend fun login(@Body body: LoginRequest): Response<TokenPair>

    @POST("v1/auth/refresh")
    suspend fun refresh(@Body body: RefreshRequest): Response<TokenPair>

    @GET("v1/farms")
    suspend fun farms(): Response<List<Farm>>

    @POST("v1/me/push-token")
    suspend fun registerPushToken(@Body body: PushTokenRequest): Response<Unit>

    @GET("v1/farms/{farmId}/incubators")
    suspend fun incubators(@Path("farmId") farmId: String): Response<List<Incubator>>

    @GET("v1/farms/{farmId}/incubators/{id}")
    suspend fun incubator(
        @Path("farmId") farmId: String,
        @Path("id") id: String,
    ): Response<Incubator>

    @GET("v1/farms/{farmId}/batches")
    suspend fun batches(
        @Path("farmId") farmId: String,
        @Query("status") status: String? = null,
    ): Response<List<Batch>>

    @POST("v1/farms/{farmId}/batches/{batchId}/candlings")
    suspend fun recordCandling(
        @Path("farmId") farmId: String,
        @Path("batchId") batchId: String,
        @Body body: CandlingRequest,
    ): Response<CandlingResponse>

    @POST("v1/farms/{farmId}/batches/{batchId}/hatch")
    suspend fun recordHatch(
        @Path("farmId") farmId: String,
        @Path("batchId") batchId: String,
        @Body body: HatchRequest,
    ): Response<HatchResponse>

    @GET("v1/farms/{farmId}/collections")
    suspend fun collections(@Path("farmId") farmId: String): Response<List<Collection>>

    @POST("v1/farms/{farmId}/collections")
    suspend fun recordCollection(
        @Path("farmId") farmId: String,
        @Body body: CollectionRequest,
    ): Response<Collection>

    @POST("v1/farms/{farmId}/collections/{id}/discard")
    suspend fun discardEggs(
        @Path("farmId") farmId: String,
        @Path("id") id: String,
        @Body body: DiscardRequest,
    ): Response<Collection>

    @POST("v1/farms/{farmId}/incubators/{id}/config")
    suspend fun pushSetpoints(
        @Path("farmId") farmId: String,
        @Path("id") id: String,
        @Body body: SetpointRequest,
    ): Response<DeviceConfig>

    @GET("v1/farms/{farmId}/incubators/{id}/config")
    suspend fun latestConfig(
        @Path("farmId") farmId: String,
        @Path("id") id: String,
    ): Response<DeviceConfig?>
}
