#include <diploma.hpp>


void diploma::setconfig(string version)
{

    require_auth( get_self() );

    // can only have one symbol per contract
    config_index config_table(get_self(), get_self().value);
    auto config_singleton  = config_table.get_or_create( get_self(), tokenconfigs{ "cometogether"_n, version, 0 } );

    // setconfig will always update version when called
    config_singleton.version = version;
    config_table.set( config_singleton, get_self() );
}


void diploma::create(name issuer,
                       name event,
                       name ticket_name,
                       bool burnable,
                       bool sellable,
                       bool transferable,
                       asset price,
                       double max_deviation,
                       double sale_split,
                       string base_uri,
                       asset max_supply,
                       vector<name> validators)
{
    require_auth( issuer );

    check( max_deviation >= 0, "Deviation must be bigger than 0" );
    check( price.amount > 0, "amount must be positive" );
    check( price.symbol == symbol( symbol_code("EOS"), 4), "Price must be in EOS token");
    checkasset(max_supply);
    // check if issuer account exists and if split is between 0 and 1
    check( is_account( issuer ), "Issuer account does not exist" );
    check( ( sale_split <= 1.0 ) && ( sale_split >= 0.0 ), "Sale split must be between 0 and 1" );

    //get event_ticket_id (global id)
    config_index config_table( get_self(), get_self().value );
    check( config_table.exists(), "Config table does not exist" );
    auto config_singleton = config_table.get();
    auto event_ticket_id = config_singleton.event_ticket_id;

    for(auto const& val : validators){
      check(is_account( val ), "Validators accounts should exist");
    }

    event_index events_table( get_self(), get_self().value );
    auto existing_event = events_table.find( event.value );

    // Create event, if it hasn't already created
    if( existing_event == events_table.end() ) {
      events_table.emplace( get_self(), [&]( auto& ev ) {
          ev.event = event;
          ev.creator = issuer;
      });
    }

    else {
      check( existing_event->creator == issuer, "Issuer must be the creator of the event");
    }

    asset current_supply = asset( 0, symbol("CTT", max_supply.symbol.precision()));
    asset issued_supply = asset( 0, symbol("CTT", max_supply.symbol.precision()));

    stat_index tickets_stats_table( get_self(), event.value );
    auto existing_ticket_stats = tickets_stats_table.find( ticket_name.value );
    check( existing_ticket_stats == tickets_stats_table.end(), "Tickets with this name already exists in this event");
    //Create token, if it hasn't already created
    tickets_stats_table.emplace( get_self(), [&]( auto& stats ){
      stats.event_ticket_id = event_ticket_id;
      stats.issuer = issuer;
      stats.ticket_name = ticket_name;
      stats.burnable = burnable;
      stats.sellable = sellable;
      stats.transferable = transferable;
      stats.price = price;
      stats.max_deviation = max_deviation;
      stats.current_supply = current_supply;
      stats.issued_supply = issued_supply;
      stats.sale_split = sale_split;
      stats.base_uri = base_uri;
      stats.max_supply = max_supply;
      stats.validators = validators;
    });

    // successful creation of token, update category_name_id to reflect
    config_singleton.event_ticket_id++;
    config_table.set( config_singleton, get_self() );
}

void diploma::issue(name to,
                      name event,
                      name ticket_name,
                      asset quantity,
                      string relative_uri,
                      string memo)
{
    check( is_account( to ), "to account does not exist" );
    check( memo.size() <= 256, "memo has more than 256 bytes" );


    stat_index tickets_stats_table( get_self(), event.value );
    const auto& ticket_stats = tickets_stats_table.get( ticket_name.value, "Tickets with this name already exists in this event");

    //ensure that only issuer can call that action and that quantity is valid
    require_auth( ticket_stats.issuer);

    checkasset(quantity);
    string string_prop = "precision of quantity must be " + to_string(ticket_stats.max_supply.symbol.precision() );
    check( quantity.symbol == ticket_stats.max_supply.symbol, string_prop.c_str() );
    check( quantity.amount <= (ticket_stats.max_supply.amount - ticket_stats.current_supply.amount), "Cannot issue more than max supply" );

    if ( quantity.amount > 1 ) {
      asset issued_supply = ticket_stats.issued_supply;
      asset one_token = asset( 1, ticket_stats.max_supply.symbol);
      for ( uint64_t i = 1; i <= quantity.amount; i++ ) {
          mint(to, ticket_stats.issuer, event, ticket_name, issued_supply, relative_uri);
          issued_supply += one_token;
      }
    }
    else {
        mint(to, ticket_stats.issuer, event, ticket_name, ticket_stats.issued_supply, relative_uri);
    }


    add_balance(to, get_self(), event, ticket_name, ticket_stats.event_ticket_id, quantity);

    // increase current supply
    tickets_stats_table.modify( ticket_stats, same_payer, [&]( auto& s ) {
        s.current_supply += quantity;
        s.issued_supply += quantity;
    });
}

void diploma::burn(name owner,
                     vector<uint64_t> ticket_ids)
{
    require_auth(owner);

    //check( ticket_ids.size() <=  )
    lock_index lockedtickets_table( get_self(), get_self().value );
    ticket_index tickets_table( get_self(), get_self().value );

    for( auto const& ticket_id: ticket_ids ) {
      const auto& ticket = tickets_table.get( ticket_id, "Ticket doesn't exist" );
      check( ticket.owner == owner , "You must be ticket owner" );

      stat_index tickets_stats_table( get_self(), ticket.event.value );
      const auto& ticket_stats = tickets_stats_table.get( ticket.ticket_name.value, "Ticket stats not found" );

      check( ticket_stats.burnable == true, "Ticket is not burnable" );
      // Check if ticket is locked
      auto lockedticket = lockedtickets_table.find( ticket_id );
      check( lockedticket == lockedtickets_table.end(), "Ticket locked" );

      asset quantity(1, ticket_stats.max_supply.symbol);
      //decrease cuurent suppply
      tickets_stats_table.modify( ticket_stats, same_payer, [&](auto& s){
        s.current_supply -= quantity;
      });

      //lower balance from owner
      sub_balance(owner, ticket_stats.event_ticket_id, quantity);

      //erase ticket
      tickets_table.erase( ticket );
    }
}

void diploma::transfer(name from,
                         name to,
                         vector<uint64_t> ticket_ids,
                         string memo ) {

    //check( dgood_ids.size() <= 20, "max batch size of 20" );
    // ensure authorized to send from account
    check( from != to, "Cannot transfer to self" );
    require_auth( from );

    // ensure 'to' account exists
    check( is_account( to ), "to account does not exist");

    // check memo size
    check( memo.size() <= 256, "memo has more than 256 bytes" );

    changeowner( from, to, ticket_ids, memo, true );
}

void diploma::listsale(name seller,
                         name event,
                         name ticket_name,
                         vector<uint64_t> ticket_ids,
                         asset net_sale_price)
{
    require_auth(seller);

    check( net_sale_price.symbol == symbol( symbol_code("EOS"), 4), "Only accept EOS token for sale");
    ticket_index tickets_table( get_self(), get_self().value );

    for( auto const& ticket_id: ticket_ids) {
      const auto& ticket = tickets_table.get( ticket_id, "Ticket does not exist" );

      stat_index tickets_stats_table( get_self(), ticket.event.value );
      const auto& ticket_stats = tickets_stats_table.get( ticket.ticket_name.value, "Ticket stats does not exist" );

      check( ticket_stats.sellable == true, "Must be sellable" );
      check( ticket.owner == seller, "Must be ticket owner" );
      check( ticket.event == event, "Tickets must be from the same event" );
      check( ticket.ticket_name == ticket_name, "Tickets must have the same ticket name" );
      double amount = static_cast<double>(ticket_stats.price.amount) * ticket_stats.max_deviation;
      asset max_resale(amount, ticket_stats.price.symbol);
      max_resale += ticket_stats.price;
      print(" max ", max_resale);
      check( net_sale_price/ticket_ids.size() <= max_resale, "Ticket resale price must be smaller than the acceptable price");

      //Check if ticket is locked
      lock_index lockedtickets_table( get_self(), get_self().value );
      auto lockedticket = lockedtickets_table.find( ticket_id );
      check( lockedticket == lockedtickets_table.end(), "Ticket locked ");

      //add ticket to lock stats_table
      lockedtickets_table.emplace( seller, [&]( auto& l ){
        l.ticket_id = ticket_id;
      });
    }

    ask_index asks_table( get_self(), get_self().value );
    //add batch to table of asks
    asks_table.emplace( seller, [&]( auto& a ){
      a.batch_id = ticket_ids[0];
      a.ticket_ids = ticket_ids;
      a.event = event;
      a.seller = seller;
      a.ask_price = net_sale_price;
      a.expiration = time_point_sec(current_time_point()) + WEEK_SEC;
    });
}

void  diploma::closesale( name seller,
                            uint64_t batch_id)
{
    ask_index asks_table( get_self(), get_self().value );
    const auto& ask = asks_table.get( batch_id, "Cannot find the desirable sale" );

    lock_index lockedtickets_table( get_self(), get_self().value );

    if( time_point_sec(current_time_point()) > ask.expiration ) {
      for( auto const& ticket_id: ask.ticket_ids ) {
        const auto& lockedticket = lockedtickets_table.get( ticket_id, "Ticket not found in lock table" );
        lockedtickets_table.erase( lockedticket );
      }
      asks_table.erase( ask );
    }
    else {
      require_auth( seller );
      check( ask.seller == seller, "Only seller can cancel a sale in progress" );
      for( auto const& ticket_id : ask.ticket_ids ) {
        const auto& lockedticket = lockedtickets_table.get( ticket_id, "Ticket is not found in lock table" );
        lockedtickets_table.erase( lockedticket );
      }
      asks_table.erase( ask );
    }
}

void diploma::buy(name from,
                    name to,
                    asset quantity,
                    string memo)
{

  if( to != get_self() ) return;
  if( from == name("eosio.stake") ) return;
  if( memo == "issue" ) return;
  check( quantity.symbol == symbol( symbol_code("EOS"), 4), "Price must be in EOS token");
  check( memo.length() <= 32, "Memo should be less than 32 bytes" );


  //memo (batch_id, to_account)
  uint64_t batch_id;
  name to_account;
  tie( batch_id, to_account ) = parsememo(memo);

  ask_index asks_table( get_self(), get_self().value );
  const auto& ask = asks_table.get( batch_id, "Cannot find listing" );
  check( ask.seller == to_account, "Seller must be the 'to' account of the memo of the transfer");
  check( ask.ask_price.amount == quantity.amount, "Should send the correct amount" );
  check( ask.expiration > time_point_sec(current_time_point()), "Sale has expired" );

  changeowner( ask.seller, from, ask.ticket_ids, "bought by: " + to_account.to_string(), false);

  map<name, asset> fee_map = calcfees(ask.ticket_ids, ask.ask_price, ask.seller);
  for( auto const& fee: fee_map ) {
    auto account = fee.first;
    auto amount = fee.second;

    //
    if( account != get_self() ) {
      //send the fees to the accounts
      action( permission_level{ get_self(), name("active") },
              name("eosio.token"),
              name("transfer"),
              make_tuple( get_self(), account, amount, string("Ticket sale"))).send();
    }
  }

    lock_index lockedtickets_table( get_self(), get_self().value );

    for( auto const& ticket_id : ask.ticket_ids ) {
      const auto& lockedticket = lockedtickets_table.get( ticket_id, "Ticket not found in lock table" );
      lockedtickets_table.erase( lockedticket );
    }

    //remove sale listing
    asks_table.erase( ask );

}


void diploma::invalidate( name validator,
                            name event,
                            name owner,
                            uint64_t ticket_id,
                            string validation_stamp_uri )
{
  check( is_account( validator ), "Validator account does not exist" );
  require_auth(validator);

  bool flag=false;
  ticket_index tickets_table( get_self(), get_self().value );
  const auto& ticket = tickets_table.get( ticket_id, "Ticket does not exist" );
  check( ticket.valid == true, "Ticket is not valid" );


  stat_index tickets_stats_table( get_self(), event.value );
  const auto& ticket_stat = tickets_stats_table.get( ticket.ticket_name.value , "Ticket stats does not exist" );
  for( auto const& val : ticket_stat.validators) {
    if( val == validator ){
      flag= true;
      break;
    }
  }

  check( flag , "You must be validator");

  tickets_table.modify( ticket, same_payer, [&]( auto& t ){
    t.valid = false;
    t.relative_uri = validation_stamp_uri;
  });

}


//Private methods
map<name, asset> diploma::calcfees( vector<uint64_t> ticket_ids, const asset& ask_amount, const name& seller ) {
  map<name, asset> fee_map;
  ticket_index tickets_table( get_self(), get_self().value );
  int64_t tot_fees = 0;
  for ( auto const& ticket_id : ticket_ids ) {
    const auto& ticket = tickets_table.get( ticket_id, "Ticket not found" );

    stat_index tickets_stats_table( get_self(), ticket.event.value );
    const auto& ticket_stat = tickets_stats_table.get( ticket.ticket_name.value, "Ticket stats not found" );

    if( ticket_stat.sale_split == 0.0 ) {
      continue;
    }

    double fee = static_cast<double>(ask_amount.amount) * ticket_stat.sale_split / static_cast<double>( ticket_ids.size() );
    asset fee_asset(fee, ask_amount.symbol);
    auto value = fee_map.insert({ticket_stat.issuer, fee_asset});
    tot_fees += fee_asset.amount;
    if( value.second == false ) {
      fee_map[ticket_stat.issuer] += fee_asset;
    }
  }

  asset seller_comp(ask_amount.amount - tot_fees, ask_amount.symbol);
  auto value = fee_map.insert({seller, seller_comp});
  if (value.second == false) {
    fee_map[seller] += seller_comp;
  }

  return fee_map;
}

void diploma::changeowner(const name& from, const name& to, vector<uint64_t> ticket_ids, const string& memo, bool istransfer) {

  ticket_index tickets_table(get_self(), get_self().value);
  lock_index lockedtickets_table(get_self(), get_self().value);

  for( auto const& ticket_id : ticket_ids ) {
    const auto& ticket = tickets_table.get( ticket_id, "Ticket not found");

    stat_index tickets_stats_table(get_self(), ticket.event.value );
    const auto& ticket_stat = tickets_stats_table.get(ticket.ticket_name.value, "Ticket stats not found");

    if( istransfer ) {
      check( ticket.owner == from, "Must be the owner");
      check( ticket_stat.transferable == true, "Not transferable");
      auto locked_ticket = lockedtickets_table.find( ticket_id);
      check( locked_ticket == lockedtickets_table.end(), "Ticket is locked, so it cannot transferred");
    }

    require_recipient( from );
    require_recipient( to );
    tickets_table.modify( ticket, same_payer, [&] ( auto& t ){
      t.owner = to;
    });

    asset quantity( 1, ticket_stat.max_supply.symbol );
    sub_balance( from, ticket_stat.event_ticket_id, quantity );
    add_balance( to, get_self(), ticket.event, ticket.ticket_name, ticket_stat.event_ticket_id, quantity );
  }
}

void diploma::checkasset(const asset& amount) {
  auto sym = amount.symbol;
  symbol_code req = symbol_code("CTT");
  check( sym.precision() == 0, "Symbol must be an int, with precision of 0" );
  check( amount.amount >= 1, "Amount must be >=1");
  check( sym.code().raw() == req.raw(), "Symbol must be CTT" );
  check( amount.is_valid(), "Invalid amount");
}

void diploma::mint(const name& to, const name& issuer, const name& event, const name& ticket_name, const asset& issued_supply, const string& relative_uri)
{
  ticket_index tickets_table( get_self(), get_self().value);
  auto ticket_id = tickets_table.available_primary_key();

  if( relative_uri.empty() ) {
    tickets_table.emplace( issuer, [&]( auto& t ){
      t.id = ticket_id;
      t.serial_number = issued_supply.amount + 1;
      t.event = event;
      t.owner = to;
      t.ticket_name = ticket_name;
      t.valid = true;
    });
  } else {
      tickets_table.emplace( issuer, [&]( auto& t ) {
        t.id = ticket_id;
        t.serial_number = issued_supply.amount + 1;
        t.event = event;
        t.owner = to;
        t.ticket_name = ticket_name;
        t.valid = true;
        t.relative_uri = relative_uri;
      });
    }
}

void diploma::add_balance(const name& owner, const name& ram_payer, const name& event, const name& ticket_name, const uint64_t& event_ticket_id, const asset& quantity )
{
  account_index to_acnts( get_self(), owner.value );
  auto to = to_acnts.find( event_ticket_id );
  if( to == to_acnts.end() ) {
    to_acnts.emplace( ram_payer, [&]( auto& a ){
      a.event_ticket_id = event_ticket_id;
      a.event = event;
      a.ticket_name = ticket_name;
      a.amount = quantity;
    });
  } else {
    to_acnts.modify( to, same_payer, [&]( auto& a ){
      a.amount += quantity;
    });
  }
}

void diploma::sub_balance(const name& owner, const uint64_t& event_ticket_id, const asset& quantity)
{
  account_index from_acnts( get_self(), owner.value );
  auto from = from_acnts.find( event_ticket_id );
  check( from->amount.amount >= quantity.amount, "Quantity must be equal or less than account balance" );

  if( from->amount.amount == quantity.amount ) {
    from_acnts.erase(from);
  } else {
    from_acnts.modify( from, same_payer, [&]( auto& a ){
      a.amount -= quantity;
    });
  }
}
