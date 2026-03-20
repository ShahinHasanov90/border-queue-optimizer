/*
 * Border Queue Optimizer
 * Queue theory models: M/M/1, M/M/c, Little's Law, Erlang-C
 *
 * All rates are in vehicles per hour.
 *
 * Copyright 2022 Shahin Hasanov
 * Licensed under the Apache License, Version 2.0
 */

#ifndef BQO_QUEUE_MODEL_H
#define BQO_QUEUE_MODEL_H

#include "../include/common.h"

/* ── M/M/1 model ─────────────────────────────────────────────────────
 * Single-server queue with Poisson arrivals, exponential service.
 *
 * lambda  = arrival rate   (vehicles/hour)
 * mu      = service rate   (vehicles/hour)
 * rho     = lambda / mu    (traffic intensity, must be < 1)
 * ──────────────────────────────────────────────────────────────────── */

/* Traffic intensity rho = lambda / mu.                                 */
double mm1_utilization(double lambda, double mu);

/* Average number of customers in the system  L = rho / (1 - rho).     */
double mm1_avg_customers(double lambda, double mu);

/* Average number in queue  Lq = rho^2 / (1 - rho).                    */
double mm1_avg_queue_length(double lambda, double mu);

/* Average time in system  W = 1 / (mu - lambda).                      */
double mm1_avg_time_in_system(double lambda, double mu);

/* Average waiting time in queue  Wq = rho / (mu - lambda).            */
double mm1_avg_wait_time(double lambda, double mu);

/* ── M/M/c model ─────────────────────────────────────────────────────
 * c identical parallel servers.
 *
 * rho = lambda / (c * mu)   must be < 1 for stability.
 * ──────────────────────────────────────────────────────────────────── */

/* Traffic intensity per server.                                        */
double mmc_utilization(double lambda, double mu, int c);

/* Erlang-C probability: probability that an arriving customer
 * must wait (all servers busy).
 *   P(wait) = [ (c*rho)^c / c! * 1/(1-rho) ]
 *             / [ sum_{k=0}^{c-1} (c*rho)^k/k! + (c*rho)^c/c! * 1/(1-rho) ]
 */
double mmc_erlang_c(double lambda, double mu, int c);

/* Average queue length  Lq = P(wait) * rho / (1 - rho).               */
double mmc_avg_queue_length(double lambda, double mu, int c);

/* Average waiting time in queue  Wq = Lq / lambda.                    */
double mmc_avg_wait_time(double lambda, double mu, int c);

/* Average number of customers in system  L = Lq + lambda/mu.          */
double mmc_avg_customers(double lambda, double mu, int c);

/* Average time in system  W = Wq + 1/mu.                              */
double mmc_avg_time_in_system(double lambda, double mu, int c);

/* ── Little's Law ────────────────────────────────────────────────────
 *   L = lambda * W
 * Given any two of { L, lambda, W }, compute the third.
 * ──────────────────────────────────────────────────────────────────── */
double little_L(double lambda, double W);
double little_lambda(double L, double W);
double little_W(double L, double lambda);

/* ── Probability that wait exceeds t ─────────────────────────────────
 *   P(Wait > t) = P(wait) * exp(-c*mu*(1-rho)*t)       (M/M/c)
 * ──────────────────────────────────────────────────────────────────── */
double mmc_prob_wait_exceeds(double lambda, double mu, int c, double t);

#endif /* BQO_QUEUE_MODEL_H */
