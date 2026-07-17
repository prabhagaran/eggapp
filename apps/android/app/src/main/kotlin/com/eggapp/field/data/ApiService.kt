package com.eggapp.field.data

import retrofit2.Response
import retrofit2.http.Body
import retrofit2.http.GET
import retrofit2.http.POST
import retrofit2.http.Path

interface ApiService {
    @POST("v1/auth/login")
    suspend fun login(@Body body: LoginRequest): Response<TokenPair>

    @POST("v1/auth/refresh")
    suspend fun refresh(@Body body: RefreshRequest): Response<TokenPair>

    @GET("v1/farms")
    suspend fun farms(): Response<List<Farm>>

    @GET("v1/farms/{farmId}/incubators")
    suspend fun incubators(@Path("farmId") farmId: String): Response<List<Incubator>>
}
