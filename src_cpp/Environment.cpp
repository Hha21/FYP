#include "../include_cpp/Environment.h"

std::tuple<std::vector<double>, double, bool> Environment::step(const double action) {

    // double new_action = std::clamp(this->last_action + action, 0.0, this->tax_limit);

    double new_action;

    // ENSURE WITHIN LIMITS
    if (this->last_action + action > this->tax_limit) {
        new_action = this->tax_limit;
    } else if (this->last_action + action < 0.0) {
        new_action = 0.0;
    } else {
        new_action = this->last_action + action;
    }

    int period = this->params.t_period;

    // DYNAMICS WITHIN PERIOD
    for (int i = 0; i < period && this->t <= this->params.T; ++i, ++this->t) {
        this->tax_actions.push_back(new_action);
        this->sector.applyExpectations(t);
        this->sector.applyAbatement(t, new_action);
        this->sector.applyProduction(t, new_action);
        this->sector.tradeCommodities(t);
    }

    // GET OBSERVATIONS
    std::vector<double> next_obs = Environment::observe();

    // GET REWARD
    double reward = Environment::calculateReward(next_obs, new_action, last_action);
    last_action = new_action;
    
    done = (t > this->params.T);

    // if (done) {
    //     Environment::outputTxt();
    // }

    // RETURN MDP
    this->Markov = {next_obs, reward, done};
    return this->Markov;
}

std::vector<double> Environment::observe() {
    int period = this->params.t_period;
    int start = this->t - period;

    double LE = this->sector.E[this->t - 1];
    double LT = this->last_action;

    // TOTAL EMISSIONS IN PERIOD
    double E_period = 0.0;

    // HHI MARKET CONCENTRATION
    double HHI = 0.0;

    // Avg. PROFIT RATE (PL), Avg. SALES PRICE (CC0)
    double numerator_PL = 0.0, denominator_PL = 0.0, CC0 = 0.0;

    for (int i = start; i < this->t; ++i) {
        E_period += this->sector.E[i];
        for (const Firm& firm : this->sector.firms) {
            HHI += firm.s[i] * firm.s[i];
            double unit_cost = this->last_action * firm.A[i] + firm.B[i];
            numerator_PL += firm.qg_s[i] * (firm.pg[i] - unit_cost);
            denominator_PL += firm.qg[i] * unit_cost;
            CC0 += firm.s[i] * firm.pg[i];
        }
    }
    double PL = (denominator_PL > 0) ? numerator_PL / denominator_PL : 0.0;

    std::vector<double> obs = {LE, LT, E_period, HHI, PL, CC0};

    for (int i = 0; i < 6; ++i) {
        obs[i] /= this->max_vals[i];
    }
    
    return obs;
}

double Environment::calculateReward(const std::vector<double>& obs, const double action, const double last_action) {
    double emissions = obs[2];
    double target = 0.2;
    double deviation = emissions - target;

    double emissions_reward = std::exp(10 * std::abs(deviation)) + 1;
    double smoothness_penalty = -0.2 * (action - last_action) * (action - last_action);

    return emissions_reward + smoothness_penalty;
}

std::vector<double> Environment::reset() {
    this->params = Parameters();
    this->sector = Sector(this->params);

    this->t = this->params.t_start;
    this->done = false;
    this->last_action = 0.0;

    auto MDP_init = Environment::step(0.0);
    std::vector<double> obs_init = Environment::observe();

    return obs_init;
}

Environment::Environment() : params(), sector(params) {
    std::cout << "C++ ENVIRONMENT INTIALISED WITH " << this->params.N << " FIRMS..." << std::endl;
    std::vector<double> init_obs = Environment::reset();   
}

void Environment::outputTxt() {
    const std::string fileName = "EmissionsVsTaxData.txt";
    this->emissionsVsTax.open(fileName);
    this->emissionsVsTax << "EMISSIONS TAX\n";

    int start = this->params.t_start;
    int end = this->params.T;

    for (int i = start; i < end; ++i) {
        this->emissionsVsTax << this->sector.E[i] << " " 
                                << this->tax_actions[i] << "\n";
    }
    this->emissionsVsTax.close();
    std::cout << "DATA WRITTEN TO " << fileName << std::endl;
}