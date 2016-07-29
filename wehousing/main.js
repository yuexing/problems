$(document).ready(function() {
    renderList(data.apartments);

    var reRender = function(thiz, sortFn) {
        thiz.addClass('chosen').siblings().removeClass('chosen');
        var arr = $('.apt').sort(sortFn);

        $.each(arr, function(idx, row) {
            var num = 'list' + (Math.floor(idx / 4) + 1),
                tab = $('#' + num);
            tab.append(row);

        });
    }

    $('#popu').click(function(e) {
        var sortByPopu = function(a, b) {
            return parseInt($(b).find(".popu").text()) - parseInt($(a).find(".popu").text());
        };
        reRender($(this), sortByPopu)
    });

    $('#price').click(function() {
        var sortByPrice = function(a, b) {
            return parseInt($(b).find(".price").text().replace(/[^\d.]/g, '')) - parseInt($(a).find(".price").text().replace(/[^\d.]/g, ''));
        };
        reRender($(this), sortByPrice);
    });

    function renderList(apts) {
        $('span.number').html(apts.length + ' Apartments');

        $.each(apts, function(idx, apt) {
            var num = 'list' + (Math.floor(idx / 4) + 1),
                tab = $('#' + num),
                name = $("<h3></h3>").attr('class', 'name').text(apt.name),
                price = $("<h3></h3>").attr('class', 'price').text('$' + apt.price),
                popularity = $("<h3></h3>").attr('class', 'popu').text(apt.popularity),
                desc = $("<p></p>").attr('class', 'desc').text(apt.description),
                img = $("<img />").attr('src', apt.image),
                btn = $('<a></a>').attr('class', 'button').attr('href', 'detail.html?id=' + apt.id).text('View Details'),
                leftCol = $("<div></div>").attr('class', 'col-md-6 col-xs-12').append(img),
                rightCol = $("<div></div>").attr('class', 'col-md-6 col-xs-12').append(name, desc, btn, price, popularity),
                row = $("<div></div>").attr('class', 'row apt').append(leftCol, rightCol);
            tab.append(row);

        });
    }

});
