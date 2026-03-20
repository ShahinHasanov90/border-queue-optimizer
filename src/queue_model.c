/*
 * Border Queue Optimizer
 * Queue theory model implementations
 *
 * M/M/1 and M/M/c queueing models with Erlang-C formula.
 * All formulas assume steady-state (rho < 1).  Functions return INFINITY
 * or NAN when the system is unstable.
 *
 * References:
 *   Gross, D., & Shortle, J. F.  Fundamentals of Queueing Theory (4th ed.)
 *   Erlang, A. K. (1917).  Solution of some problems in the theory of
 *       probabilities of significance in automatic telephone exchanges.
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#include "queue_model.h"
#include <math.h>

/* ── M/M/1 ───────────────────────────────────────────────────────────── */

double mm1_utilization(double lambda, double mu)
{
    if (mu <= BQO_EPSILON)
        return INFINITY;
    return lambda / mu;
}

double mm1_avg_customers(double lambda, double mu)
{
    double rho = mm1_utilization(lambda, mu);
    if (rho >= 1.0)
        return INFINITY;
    return rho / (1.0 - rho);
}

double mm1_avg_queue_length(double lambda, double mu)
{
    double rho = mm1_utilization(lambda, mu);
    if (rho >= 1.0)
        return INFINITY;
    return (rho * rho) / (1.0 - rho);
}

double mm1_avg_time_in_system(double lambda, double mu)
{
    if (mu <= lambda)
        return INFINITY;
    return 1.0 / (mu - lambda);
}

double mm1_avg_wait_time(double lambda, double mu)
{
    double rho = mm1_utilization(lambda, mu);
    if (rho >= 1.0)
        return INFINITY;
    return rho / (mu - lambda);
}

/* ── Helper: compute (c*rho)^k / k! iteratively to avoid overflow ──── */

static double sum_poisson_terms(double a, int n)
{
    /* Compute sum_{k=0}^{n} a^k / k! */
    double term = 1.0;  /* k=0 */
    double sum  = 1.0;
    for (int k = 1; k <= n; k++) {
        term *= a / (double)k;
        sum  += term;
    }
    return sum;
}

static double poisson_term(double a, int n)
{
    /* Compute a^n / n! */
    double term = 1.0;
    for (int k = 1; k <= n; k++) {
        term *= a / (double)k;
    }
    return term;
}

/* ── M/M/c ───────────────────────────────────────────────────────────── */

double mmc_utilization(double lambda, double mu, int c)
{
    if (c <= 0 || mu <= BQO_EPSILON)
        return INFINITY;
    return lambda / ((double)c * mu);
}

/*
 * Erlang-C formula
 *
 *              (c * rho)^c / c!  *  1/(1 - rho)
 * C(c, a) = ─────────────────────────────────────
 *            sum_{k=0}^{c-1} (c*rho)^k/k!  +  (c*rho)^c/c! * 1/(1-rho)
 *
 * where a = c * rho = lambda / mu.
 */
double mmc_erlang_c(double lambda, double mu, int c)
{
    double rho = mmc_utilization(lambda, mu, c);
    if (rho >= 1.0)
        return 1.0;  /* all arrivals wait in an overloaded system */
    if (c <= 0)
        return NAN;

    double a = lambda / mu;  /* total offered load */

    double last_term = poisson_term(a, c);      /* a^c / c! */
    double last_term_adj = last_term / (1.0 - rho);

    double sum = sum_poisson_terms(a, c - 1);

    return last_term_adj / (sum + last_term_adj);
}

double mmc_avg_queue_length(double lambda, double mu, int c)
{
    double rho = mmc_utilization(lambda, mu, c);
    if (rho >= 1.0)
        return INFINITY;

    double pc = mmc_erlang_c(lambda, mu, c);
    return pc * rho / (1.0 - rho);
}

double mmc_avg_wait_time(double lambda, double mu, int c)
{
    if (lambda <= BQO_EPSILON)
        return 0.0;
    double lq = mmc_avg_queue_length(lambda, mu, c);
    if (isinf(lq))
        return INFINITY;
    return lq / lambda;         /* Little's law: Wq = Lq / lambda */
}

double mmc_avg_customers(double lambda, double mu, int c)
{
    double lq = mmc_avg_queue_length(lambda, mu, c);
    if (isinf(lq))
        return INFINITY;
    return lq + lambda / mu;    /* L = Lq + lambda/mu */
}

double mmc_avg_time_in_system(double lambda, double mu, int c)
{
    double wq = mmc_avg_wait_time(lambda, mu, c);
    if (isinf(wq))
        return INFINITY;
    return wq + 1.0 / mu;      /* W = Wq + 1/mu */
}

/* ── Little's Law ────────────────────────────────────────────────────── */

double little_L(double lambda, double W)
{
    return lambda * W;
}

double little_lambda(double L, double W)
{
    if (W <= BQO_EPSILON)
        return 0.0;
    return L / W;
}

double little_W(double L, double lambda)
{
    if (lambda <= BQO_EPSILON)
        return 0.0;
    return L / lambda;
}

/* ── Tail probability ────────────────────────────────────────────────── */

double mmc_prob_wait_exceeds(double lambda, double mu, int c, double t)
{
    double rho = mmc_utilization(lambda, mu, c);
    if (rho >= 1.0)
        return 1.0;

    double pc = mmc_erlang_c(lambda, mu, c);
    /* P(Wait > t) = C(c,a) * exp(-c*mu*(1-rho)*t) */
    return pc * exp(-(double)c * mu * (1.0 - rho) * t);
}
