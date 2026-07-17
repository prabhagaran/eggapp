package com.eggapp.field.data

import com.eggapp.field.BuildConfig
import kotlinx.coroutines.runBlocking
import okhttp3.Authenticator
import okhttp3.Interceptor
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.Response
import okhttp3.Route
import okhttp3.logging.HttpLoggingInterceptor
import retrofit2.Retrofit
import retrofit2.converter.gson.GsonConverterFactory

/**
 * Attaches the stored access token to every request. Login/refresh/setup
 * calls skip this (no token exists yet / would be circular) — those go
 * through a second, auth-free Retrofit instance ([ApiClient.authFree]).
 */
private class AuthInterceptor(private val tokenStore: TokenStore) : Interceptor {
    override fun intercept(chain: Interceptor.Chain): Response {
        val token = tokenStore.accessToken()
        val request = if (token != null) {
            chain.request().newBuilder().addHeader("Authorization", "Bearer $token").build()
        } else {
            chain.request()
        }
        return chain.proceed(request)
    }
}

/**
 * OkHttp Authenticator: on a 401, attempts one refresh-and-retry — mirrors
 * the web client's tryRefresh-then-retry-once in apps/web/lib/api.ts.
 * Synchronous (Authenticator contract), hence runBlocking for the one
 * refresh call.
 */
private class RefreshAuthenticator(
    private val tokenStore: TokenStore,
    private val authFreeApi: ApiService,
) : Authenticator {
    override fun authenticate(route: Route?, response: Response): Request? {
        // Already retried once for this request — give up rather than loop.
        if (response.request.header("X-Retry") != null) return null

        val refreshToken = tokenStore.refreshToken() ?: return null
        val newTokens = runBlocking {
            runCatching { authFreeApi.refresh(RefreshRequest(refreshToken)) }.getOrNull()
        }?.body() ?: return null

        tokenStore.saveTokens(newTokens.accessToken, newTokens.refreshToken)
        return response.request.newBuilder()
            .header("Authorization", "Bearer ${newTokens.accessToken}")
            .header("X-Retry", "1")
            .build()
    }
}

object ApiClient {
    private fun retrofit(client: OkHttpClient): Retrofit =
        Retrofit.Builder()
            .baseUrl(BuildConfig.API_BASE_URL)
            .client(client)
            .addConverterFactory(GsonConverterFactory.create())
            .build()

    /** Auth-free client — used for login/refresh, and internally by the authenticator. */
    fun authFree(): ApiService {
        val client = OkHttpClient.Builder()
            .addInterceptor(HttpLoggingInterceptor().setLevel(HttpLoggingInterceptor.Level.BASIC))
            .build()
        return retrofit(client).create(ApiService::class.java)
    }

    /** Authenticated client — attaches the bearer token, auto-refreshes once on 401. */
    fun authenticated(tokenStore: TokenStore): ApiService {
        val client = OkHttpClient.Builder()
            .addInterceptor(AuthInterceptor(tokenStore))
            .authenticator(RefreshAuthenticator(tokenStore, authFree()))
            .addInterceptor(HttpLoggingInterceptor().setLevel(HttpLoggingInterceptor.Level.BASIC))
            .build()
        return retrofit(client).create(ApiService::class.java)
    }
}
